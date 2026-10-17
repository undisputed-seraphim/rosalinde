#include "runtime_state.hpp"
#include "scene_layers.hpp"
#include "scene_util.hpp"
#include "screenshot.hpp"
#include "tables.hpp"
#include "trace.hpp"

#include <criware/cpk.hpp>
#include <scarlett/asb.hpp>
#include <scarlett/host.hpp>

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {

static bool scene_trace() {
	static const bool on = std::getenv("SCENE_TRACE") != nullptr;
	return on;
}

// Read the default framebuffer (the window / offscreen pbuffer) into a PNG.
void write_framebuffer(const std::filesystem::path& path) {
	int fb_w = 0, fb_h = 0;
	SDL_GetWindowSizeInPixels(SDL_GL_GetCurrentWindow(), &fb_w, &fb_h);
	if (fb_w <= 0 || fb_h <= 0)
		return;
	std::vector<uint8_t> pixels(static_cast<size_t>(fb_w) * fb_h * 4);
	glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	const bool ok = screenshot::write(path, fb_w, fb_h, pixels);
	fprintf(stderr, "%s: %s (%dx%d)\n", ok ? "capture" : "capture FAILED", path.c_str(), fb_w, fb_h);
}

// Length (in animation frames) of a track: the longest sub-animation (the MBS
// sequences of a track are body parts that play together, so the track ends
// when its longest part completes one cycle). Cached; 0 when unknown, which
// leaves `WaitSpriteMoneAnimationEnd` immediate.
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
				if (std::getenv("S8_ALL")) {
					static std::unordered_set<std::string> dumped;
					if (dumped.insert(mbs).second) {
						for (uint32_t i = 0; i < sd.v77.s8.size(); ++i) {
							const auto& e = sd.v77.s8[i];
							std::fprintf(
								stderr,
								"[s8all %s] i=%u flags=%#x frames=%u loop=%u s6=%u s7=%u\n",
								mbs.c_str(),
								i,
								e.flags,
								e.frames,
								e.loop_s8_id,
								e.s6_id,
								e.s7_id);
						}
					}
				}
				if (track_index < static_cast<int>(sd.tracks.size())) {
					for (const auto& run : sd.tracks[track_index].runs) {
						long run_len = 0;
						for (uint32_t k = 0; k < run.s8_count && run.s8_start + k < sd.v77.s8.size(); ++k)
							run_len += sd.v77.s8[run.s8_start + k].frames;
						if (std::getenv("TRACK_DUMP"))
							std::fprintf(
								stderr,
								"[track %s idx=%d main=%d] run s8_start=%u count=%u once=%d track_id=%u len=%ld\n",
								mbs.c_str(),
								track_index,
								sd.tracks[track_index].sa_set_main,
								run.s8_start,
								run.s8_count,
								run.once ? 1 : 0,
								run.track_id,
								run_len);
						if (std::getenv("S8_DUMP"))
							for (uint32_t k = 0; k < run.s8_count && run.s8_start + k < sd.v77.s8.size(); ++k) {
								const auto& e = sd.v77.s8[run.s8_start + k];
								std::fprintf(
									stderr,
									"[s8 %s idx=%d] k=%u flags=%#x frames=%u loop=%u s6=%u\n",
									mbs.c_str(),
									track_index,
									k,
									e.flags,
									e.frames,
									e.loop_s8_id,
									e.s6_id);
							}
						n = std::max(n, run_len);
					}
				}
			}
		}
	} catch (...) {
		n = 0;
	}
	cache[key] = n;
	return n;
}

// Index of the MBS track named `name` for `mbs`, or -1 when unknown. Cached;
// backs the hosted-VM animation-name resolver (DrSetIdleAnimation/DrAnimation*).
int mbs_resolve_track(const AssetLoader& loader, const std::string& mbs, const std::string& name) {
	if (mbs.empty() || name.empty())
		return -1;
	static std::unordered_map<std::string, int> cache;
	const std::string key = mbs + '\x1f' + name;
	if (auto it = cache.find(key); it != cache.end())
		return it->second;

	int idx = -1;
	try {
		if (auto p = loader.resolve(mbs)) {
			const CPKTable& cpk = loader.cpk();
			if (auto e = cpk.find_file(p->dir, p->path); e != cpk.end()) {
				std::vector<char> buf;
				cpk.extract(*e, buf);
				SpriteData sd = SpriteData::load(
					std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buf.data()), buf.size()), {});
				const SpriteData::Track* t = sd.find_track(name);
				if (t)
					idx = static_cast<int>(t - sd.tracks.data());
			}
		}
	} catch (...) {
		idx = -1;
	}
	cache[key] = idx;
	return idx;
}

} // namespace

RuntimeState::RuntimeState(const AssetLoader& loader, RuntimeSceneConfig cfg)
	: _loader(loader)
	, _cfg(std::move(cfg))
	, _camera(1.0f) {

	_projection = glm::ortho(-960.0f, 960.0f, 540.0f, -540.0f);
	if (const char* o = std::getenv("VIEWER_ORTHO")) {
		float a = 0, b = 0, c = 0, d = 0;
		if (std::sscanf(o, "%f,%f,%f,%f", &a, &b, &c, &d) == 4)
			_projection = glm::ortho(a, b, c, d);
	}
	enable_blend(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	enable_depth(GL_ALWAYS);

	// Throwaway capture harness: VIEWER_CAPTURE=dir:from:to:every steps one
	// scene frame per tick (deterministic) and writes each sampled frame.
	if (const char* c = std::getenv("VIEWER_CAPTURE")) {
		std::string s(c), part;
		std::vector<std::string> parts;
		for (char ch : s + ':') {
			if (ch == ':') {
				parts.push_back(part);
				part.clear();
			} else {
				part += ch;
			}
		}
		if (parts.size() >= 3) {
			_cap_dir = parts[0];
			_cap_from = std::atol(parts[1].c_str());
			_cap_to = std::atol(parts[2].c_str());
			_cap_every = parts.size() >= 4 && !parts[3].empty() ? std::max(1L, std::atol(parts[3].c_str())) : 1;
			std::fprintf(stderr, "[capture] dir=%s frames=%ld..%ld every=%ld\n", _cap_dir.c_str(), _cap_from, _cap_to, _cap_every);
		}
	}

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
	asb::CharaSetTable chara_sets = asb::load_chara_set(
		viewer_data_dir() + "/charaset.tsv",
		viewer_data_dir() + "/classes.tsv",
		viewer_data_dir() + "/charanpc_assets.tsv");

	asb::Host host;
	host.set_call_log([](const std::string&) {});
	if (!chars.empty())
		host.set_char_table(chars);
	host.set_chara_set_table(chara_sets);
	host.set_track_frames([this](const std::string& mbs, int idx) { return mbs_track_frames(_loader, mbs, idx); });
	host.set_resolve_track(
		[this](const std::string& mbs, const std::string& name) { return mbs_resolve_track(_loader, mbs, name); });
	asb::FacilityNames facilities = asb::load_facility_names(viewer_data_dir() + "/uc_enums.tsv");
	host.set_facility_name([facilities](const std::string& key) { return facilities.resolve(key); });

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
		for (const auto& f : _timeline.fades)
			std::fprintf(
				stderr,
				"[fade %ld..%ld] (%.2f,%.2f,%.2f,%.2f) -> (%.2f,%.2f,%.2f,%.2f)\n",
				f.start,
				f.end,
				f.from[0],
				f.from[1],
				f.from[2],
				f.from[3],
				f.to[0],
				f.to[1],
				f.to[2],
				f.to[3]);
		for (const auto& k : _timeline.keyframes) {
			if (k.camera.has_pos)
				std::fprintf(
					stderr,
					"[cam f=%ld] pos=(%.0f,%.0f,%.0f) zoom=%.3f\n",
					k.frame,
					k.camera.pos[0],
					k.camera.pos[1],
					k.camera.pos[2],
					k.camera.zoom);
		}
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
		s.track = e.track;
		s.pos = glm::vec2(e.pos[0], e.pos[1]);
		s.scale = e.scale;
		s.visible = true; // per-frame apply_keyframe controls actual visibility
		specs.push_back(std::move(s));
	}

	_layers = build_layers(_loader, specs, _cfg.limit);
	_active.assign(_layers.size(), 0);
	_track.assign(_layers.size(), -1);

	std::unordered_map<std::string, int> by_name;
	for (size_t i = 0; i < ents.size(); ++i) {
		by_name[ents[i].name] = static_cast<int>(i);
		for (const auto& a : ents[i].aliases)
			by_name[a] = static_cast<int>(i);
	}
	_parent.assign(_layers.size(), -1);
	for (size_t i = 0; i < ents.size() && i < _parent.size(); ++i) {
		if (ents[i].parent.empty())
			continue;
		if (auto it = by_name.find(ents[i].parent); it != by_name.end() && it->second != static_cast<int>(i))
			_parent[i] = it->second;
	}

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

	if (_cfg.camera == "script") {
		for (const auto& k : _timeline.keyframes)
			if (k.camera.has_pos)
				_cam_keys.emplace_back(k.frame, glm::vec2(k.camera.pos[0], k.camera.pos[1]));
	}

	if (_cfg.camera == "record") {
		for (const auto& k : _timeline.keyframes)
			if (k.camera.has_pos)
				_cam_rec.push_back({k.frame, glm::vec2(k.camera.pos[0], k.camera.pos[1]), k.camera.zoom});
	}

	if (_cfg.camera != "fit" && _cfg.camera != "design" && _cfg.camera != "follow" &&
		_cfg.camera != "script" && _cfg.camera != "record" && _cfg.camera != "auto") {
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
		if (scene_trace() && on != static_cast<bool>(_active[i]))
			std::fprintf(stderr, "[play f=%ld] %c %s\n", _frame, on ? '+' : '-', _layers[i]->class_name.c_str());
		if (on && !_active[i])
			_focus = static_cast<int>(i);
		_active[i] = on ? 1 : 0;
		if (i >= n)
			continue;
		const asb::RuntimeEntity& e = k.entities[i];
		glm::vec2 pos(e.pos[0], e.pos[1]);
		if (_parent[i] >= 0 && _parent[i] < static_cast<int>(n))
			pos += glm::vec2(k.entities[_parent[i]].pos[0], k.entities[_parent[i]].pos[1]);
		_layers[i]->position = pos;
		_layers[i]->scale = e.scale > 0.0f ? e.scale : 1.0f;
		const int tracks = static_cast<int>(_layers[i]->data.tracks.size());
		int ti = e.track_index;
		if (ti < 0 && !e.track.empty()) {
			if (const SpriteData::Track* t = _layers[i]->data.find_track(e.track))
				ti = static_cast<int>(t - _layers[i]->data.tracks.data());
		}
		if (ti >= 0 && ti != _track[i] && ti < tracks) {
			_layers[i]->instance.play(static_cast<uint32_t>(ti));
			_track[i] = ti;
		}
	}
}

void RuntimeState::tick(float dt) {
	for (size_t i = 0; i < _layers.size(); ++i) {
		if (!_layers[i] || !_active[i])
			continue;
		if (_layers[i]->instances.empty()) {
			_layers[i]->instance.update(dt);
		} else {
			// Independent clocks: do NOT lockstep the background tracks.
			for (auto& inst : _layers[i]->instances)
				inst.update(dt);
		}
	}
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
	if (_cap_from >= 0) {
		// First tick: jump to _cap_from (the initial frame 0 is already applied).
		// After that advance one sample step (_cap_every) at a time. Intermediate
		// frames are ticked but not rendered, so sampling is cheap.
		const long target = _cap_started ? std::min(_cap_to, _frame + _cap_every) : _cap_from;
		if (_cap_started && _frame >= _cap_to) {
			_done = true;
			return;
		}
		while (_frame < target) {
			++_frame;
			apply_keyframe();
			tick(1.0f / std::max(1.0f, _cfg.fps));
		}
		_cap_started = true;
		return;
	}
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

	if (_cfg.camera == "follow" && _focus >= 0 && _focus < static_cast<int>(_layers.size()) && _layers[_focus])
		_camera.recenter(_layers[_focus]->position);
	if (_cfg.camera == "script" && !_cam_keys.empty()) {
		glm::vec2 p = _cam_keys.front().second;
		for (const auto& [f, q] : _cam_keys) {
			if (f > _frame)
				break;
			p = q;
		}
		_camera.recenter(p);
	}
	if (_cfg.camera == "record" && !_cam_rec.empty()) {
		// The game's view rect is centred on the camera pos with width 1840/zoom
		// (docs/EXE.md §4); apply the recorded pos+zoom as our view.
		const CamKey* k = &_cam_rec.front();
		for (const auto& kk : _cam_rec) {
			if (kk.frame > _frame)
				break;
			k = &kk;
		}
		const float zg = k->zoom > 0.0f ? k->zoom : 1.0f;
		float world_w = 1920.0f; // design width (FUN_000c3380 uses 1920x1080)
		if (const char* e = std::getenv("VIEWER_CAM_W"))
			world_w = static_cast<float>(std::atof(e));
		const float view_w = world_w / zg;
		const float view_h = 1080.0f / zg;
		_camera.fit_bounds(
			glm::vec4(
				k->pos.x - view_w * 0.5f,
				k->pos.y - view_h * 0.5f,
				k->pos.x + view_w * 0.5f,
				k->pos.y + view_h * 0.5f),
			0.0f);
	}
	if (_cfg.camera == "auto") {
		// Content-driven framing. Bounds come from per-frame render_bounds()
		// (s6 x s7), NOT track_bounds() which is the s9 camera-framing hint.
		// The view holds the active characters together with the largest
		// background layer (the parade reads over the landscape, like the
		// reference); with no character, the largest background layer alone.
		struct Cand {
			glm::vec4 b;
			bool character;
			float area;
		};
		std::vector<Cand> cands;
		for (size_t i = 0; i < _layers.size(); ++i) {
			if (!_layers[i] || !_active[i])
				continue;
			const SpriteLayer& l = *_layers[i];
			glm::vec4 b{INFINITY, INFINITY, -INFINITY, -INFINITY};
			bool got = false;
			auto acc = [&](const SpriteInstance& inst) {
				const glm::vec4 r = inst.render_bounds();
				if (!std::isfinite(r.x) || !std::isfinite(r.z) || r.z <= r.x || r.w <= r.y)
					return;
				const glm::vec4 w{r.x + l.position.x, r.y + l.position.y, r.z + l.position.x, r.w + l.position.y};
				b = {std::min(b.x, w.x), std::min(b.y, w.y), std::max(b.z, w.z), std::max(b.w, w.w)};
				got = true;
			};
			if (l.instances.empty())
				acc(l.instance);
			else
				for (const auto& inst : l.instances)
					acc(inst);
			if (!got)
				continue;
			const float w = b.z - b.x, h = b.w - b.y;
			if (w > 6000.0f || h > 4000.0f) // pathological / full-screen effect
				continue;
			cands.push_back({b, l.is_character, w * h});
		}
		glm::vec4 fit{INFINITY, INFINITY, -INFINITY, -INFINITY};
		bool any = false;
		auto merge = [&](const glm::vec4& b) {
			fit = {std::min(fit.x, b.x), std::min(fit.y, b.y), std::max(fit.z, b.z), std::max(fit.w, b.w)};
			any = true;
		};
		for (const auto& c : cands)
			if (c.character)
				merge(c.b);
		if (any) {
			// Include the largest background layer so the parade reads over the
			// landscape; with no character, keep the fixed design view (where
			// the cutscene cards are composed).
			float best = -1.0f;
			glm::vec4 bb{};
			for (const auto& c : cands)
				if (!c.character && c.area > best) {
					best = c.area;
					bb = c.b;
				}
			if (best > 0.0f)
				merge(bb);
		}
		if (any)
			_camera.fit_bounds(fit, 0.12f);
		else
			_camera.recenter({0.0f, 0.0f});
	}

	if (const char* fr = std::getenv("LAYER_DUMP")) {
		static bool dumped = false;
		if (!dumped && _frame >= std::atol(fr)) {
			dumped = true;
			for (size_t i = 0; i < _layers.size(); ++i)
				if (_layers[i] && _active[i]) {
					const glm::vec4 b = world_bounds(*_layers[i]);
					std::fprintf(
						stderr,
						"[layer f=%ld] %-22s pos=(%.0f,%.0f) wb=(%.0f,%.0f,%.0f,%.0f)\n",
						_frame,
						_layers[i]->class_name.c_str(),
						_layers[i]->position.x,
						_layers[i]->position.y,
						b.x,
						b.y,
						b.z,
						b.w);
				}
		}
	}

	glClear(GL_DEPTH_BUFFER_BIT);
	for (size_t i = 0; i < _layers.size(); ++i) {
		if (!_layers[i] || !_active[i])
			continue;
		auto& l = *_layers[i];
		if (l.instances.empty()) {
			l.renderer.draw(l.instance, _projection, _camera, l.position, &l.layer_tints, false, l.scale);
		} else {
			for (auto& inst : l.instances)
				l.renderer.draw(inst, _projection, _camera, l.position, &l.layer_tints, false, l.scale);
		}
	}

	float fade[4];
	asb::fade_at(_timeline.fades, _frame, fade);
	_fade.draw(glm::vec4(fade[0], fade[1], fade[2], fade[3]));

	if (!_cfg.screenshot.empty() && !_captured && _render_count >= _cfg.frames) {
		write_framebuffer(_cfg.screenshot);
		_captured = true;
	}

	if (_cap_from >= 0 && _frame >= _cap_from && _frame <= _cap_to && (_frame - _cap_from) % _cap_every == 0) {
		std::error_code ec;
		std::filesystem::create_directories(_cap_dir, ec);
		char name[32];
		std::snprintf(name, sizeof name, "f%06ld.png", _frame);
		write_framebuffer(std::filesystem::path(_cap_dir) / name);
	}
}
