#include "runtime_state.hpp"
#include "scene_layers.hpp"
#include "scene_util.hpp"
#include "screenshot.hpp"
#include "trace.hpp"

#include <criware/cpk.hpp>
#include <scarlett/asb.hpp>
#include <scarlett/host.hpp>

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <span>
#include <stdexcept>
#include <unordered_map>

namespace {

// Total length (in animation frames) of a track, from the MBS `s8[].frames`.
// Cached; 0 when unknown, which leaves `WaitSpriteMoneAnimationEnd` immediate.
long mbs_track_frames(const AssetLoader& loader, const std::string& mbs, int track_index) {
	if (mbs.empty() || track_index < 0)
		return 0;
	static std::unordered_map<std::string, long> cache;
	const std::string key = mbs + '\x1f' + std::to_string(track_index);
	if (auto it = cache.find(key); it != cache.end())
		return it->second;

	long n = 0;
	try {
		if (auto p = loader.resolve(mbs)) {
			const CPKTable& cpk = loader.cpk();
			if (auto e = cpk.find_file(p->dir, p->path); e != cpk.end()) {
				std::vector<char> buf;
				cpk.extract(*e, buf);
				SpriteData sd = SpriteData::load(
					std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buf.data()), buf.size()), {});
				if (track_index < static_cast<int>(sd.tracks.size())) {
					for (const auto& run : sd.tracks[track_index].runs)
						for (uint32_t k = 0; k < run.s8_count && run.s8_start + k < sd.v77.s8.size(); ++k)
							n += sd.v77.s8[run.s8_start + k].frames;
				}
			}
		}
	} catch (...) {
		n = 0;
	}
	cache[key] = n;
	return n;
}

} // namespace

RuntimeState::RuntimeState(const AssetLoader& loader, RuntimeSceneConfig cfg)
	: _loader(loader)
	, _cfg(std::move(cfg))
	, _camera(1.0f) {

	_projection = glm::ortho(-960.0f, 960.0f, 540.0f, -540.0f);
	enable_blend(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	enable_depth(GL_ALWAYS);

	const CPKTable& cpk = _loader.cpk();
	const auto slash = _cfg.entry.find_last_of('/');
	const std::string dir = slash == std::string::npos ? "" : _cfg.entry.substr(0, slash);
	const std::string file = slash == std::string::npos ? _cfg.entry : _cfg.entry.substr(slash + 1);

	auto entry = cpk.find_file(dir, file);
	if (entry == cpk.end())
		throw std::runtime_error("script not found in CPK: " + _cfg.entry);

	std::vector<char> buf;
	cpk.extract(*entry, buf);
	viewer::trace::stamp("RuntimeState: CPK entry extracted (%zu bytes)", buf.size());

	asb::CharTable chars;
	if (!_cfg.chara_table.empty())
		chars = asb::load_char_table(_cfg.chara_table);

	asb::Host host;
	host.set_call_log([](const std::string&) {});
	if (!chars.empty())
		host.set_char_table(chars);
	host.set_track_frames([this](const std::string& mbs, int idx) { return mbs_track_frames(_loader, mbs, idx); });

	viewer::trace::stamp("RuntimeState: VM run begin");
	_timeline = asb::run_module(
		host,
		std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buf.data()), buf.size()),
		_cfg.entry,
		_cfg.func);
	viewer::trace::stamp("RuntimeState: VM run done (%zu keyframes)", _timeline.keyframes.size());
	if (_timeline.keyframes.empty())
		throw std::runtime_error("runtime produced no timeline for " + _cfg.entry);

	fprintf(
		stderr,
		"[runtime] %s :: %s  (keyframes=%zu entities=%zu)\n",
		_cfg.entry.c_str(),
		_timeline.function.c_str(),
		_timeline.keyframes.size(),
		_timeline.keyframes.back().entities.size());

	if (std::getenv("RUNTIME_DUMP")) {
		for (const auto& k : _timeline.keyframes) {
			std::fprintf(stderr, "[kf %ld]", k.frame);
			for (const auto& e : k.entities)
				if (e.visible && e.alpha > 0)
					std::fprintf(
						stderr,
						" %s(%.0f,%.0f a=%d s=%.2f t=%d)",
						e.name.c_str(),
						e.pos[0],
						e.pos[1],
						e.alpha,
						e.scale,
						e.track_index);
			std::fprintf(stderr, "\n");
		}
	}

	const auto& ents = _timeline.keyframes.back().entities;
	std::vector<LayerSpec> specs;
	specs.reserve(ents.size());
	for (const auto& e : ents) {
		LayerSpec s;
		s.id = e.name;
		s.kind = e.kind;
		s.mbs = e.mbs;
		s.ftx = e.ftx;
		s.pos = glm::vec2(e.pos[0], e.pos[1]);
		s.scale = e.scale;
		s.visible = true; // per-frame apply_keyframe controls actual visibility
		specs.push_back(std::move(s));
	}

	_layers = build_layers(_loader, specs, _cfg.limit);
	_active.assign(_layers.size(), 0);
	_track.assign(_layers.size(), -1);

	size_t loaded = 0;
	for (const auto& l : _layers)
		if (l)
			++loaded;
	viewer::trace::stamp("RuntimeState: layers built (%zu loaded)", loaded);
	fprintf(stderr, "[runtime] loaded %zu sprite layer(s)\n", loaded);
	if (loaded == 0)
		throw std::runtime_error("scene produced no loadable sprites: " + _cfg.entry);

	glm::vec4 fit{INFINITY, INFINITY, -INFINITY, -INFINITY};
	bool any = false;
	for (const auto& l : _layers) {
		if (!l)
			continue;
		const glm::vec4 b = world_bounds(*l);
		fit = {std::min(fit.x, b.x), std::min(fit.y, b.y), std::max(fit.z, b.z), std::max(fit.w, b.w)};
		any = true;
	}
	// "design" keeps the fixed design-space projection (origin centred), which
	// matches the game's UI space; "fit" zooms to the scene bounds instead.
	if (any && _cfg.camera == "fit")
		_camera.fit_bounds(fit, 0.15f);

	if (_cfg.camera != "fit" && _cfg.camera != "design") {
		std::vector<const asb::RuntimeCamera*> keys;
		for (const auto& k : _timeline.keyframes)
			if (k.camera.has_pos)
				keys.push_back(&k.camera);
		const asb::RuntimeCamera* key = nullptr;
		if (_cfg.camera == "first" && !keys.empty())
			key = keys.front();
		else if (_cfg.camera == "last" && !keys.empty())
			key = keys.back();
		else {
			const int n = std::atoi(_cfg.camera.c_str());
			if (n >= 0 && n < static_cast<int>(keys.size()))
				key = keys[n];
		}
		if (key) {
			float s = 1.0f;
			if (const char* e = std::getenv("SCENE_CAM_SCALE"))
				s = std::atof(e);
			_camera.recenter({key->pos[0] * s, key->pos[1] * s});
			fprintf(stderr, "[runtime] camera key pos=(%.0f,%.0f)\n", key->pos[0], key->pos[1]);
		}
	}

	apply_keyframe();
	viewer::trace::stamp("RuntimeState: ready (frame 0 applied)");
}

void RuntimeState::apply_keyframe() {
	while (_kf + 1 < _timeline.keyframes.size() && _timeline.keyframes[_kf + 1].frame <= _frame)
		++_kf;
	const asb::Keyframe& k = _timeline.keyframes[_kf];
	const size_t n = k.entities.size();
	for (size_t i = 0; i < _layers.size(); ++i) {
		if (!_layers[i])
			continue;
		const bool on = i < n && k.entities[i].visible && k.entities[i].alpha > 0;
		_active[i] = on ? 1 : 0;
		if (i >= n)
			continue;
		const asb::RuntimeEntity& e = k.entities[i];
		_layers[i]->position = glm::vec2(e.pos[0], e.pos[1]);
		const int tracks = static_cast<int>(_layers[i]->data.tracks.size());
		if (e.track_index >= 0 && e.track_index != _track[i] && e.track_index < tracks) {
			_layers[i]->instance.play(static_cast<uint32_t>(e.track_index));
			_track[i] = e.track_index;
		}
	}
}

void RuntimeState::tick(float dt) {
	for (size_t i = 0; i < _layers.size(); ++i)
		if (_layers[i] && _active[i])
			_layers[i]->instance.update(dt);
}

bool RuntimeState::handle_inputs() {
	SDL_Event event{};
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
			_quit = true;
		if (event.type == SDL_EVENT_KEY_DOWN) {
			if (event.key.key == SDLK_ESCAPE)
				_done = true;
			else if (event.key.key == SDLK_SPACE)
				_playing = !_playing;
		}
	}
	return _done;
}

void RuntimeState::update(float dt) {
	if (_cfg.frame >= 0) {
		if (!_stepped) {
			while (_frame < _cfg.frame) {
				++_frame;
				apply_keyframe();
				tick(1.0f / std::max(1.0f, _cfg.fps));
			}
			_stepped = true;
		}
		return;
	}
	if (!_playing)
		return;
	_accum += static_cast<double>(dt) * std::max(1.0f, _cfg.fps);
	while (_accum >= 1.0) {
		_accum -= 1.0;
		++_frame;
		apply_keyframe();
		tick(1.0f / std::max(1.0f, _cfg.fps));
	}
}

void RuntimeState::render() {
	++_render_count;

	glClear(GL_DEPTH_BUFFER_BIT);
	for (size_t i = 0; i < _layers.size(); ++i)
		if (_layers[i] && _active[i])
			_layers[i]->renderer.draw(
				_layers[i]->instance, _projection, _camera, _layers[i]->position, &_layers[i]->layer_tints);

	if (!_cfg.screenshot.empty() && !_captured && _render_count >= _cfg.frames) {
		int fb_w = 0, fb_h = 0;
		SDL_GetWindowSizeInPixels(SDL_GL_GetCurrentWindow(), &fb_w, &fb_h);
		if (fb_w > 0 && fb_h > 0) {
			std::vector<uint8_t> pixels(static_cast<size_t>(fb_w) * fb_h * 4);
			glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
			const bool ok = screenshot::write(_cfg.screenshot, fb_w, fb_h, pixels);
			fprintf(
				stderr,
				"%s: %s (%dx%d)\n",
				ok ? "screenshot written" : "screenshot FAILED",
				_cfg.screenshot.c_str(),
				fb_w,
				fb_h);
		}
		_captured = true;
	}
}
