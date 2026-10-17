#include "drama_playback.hpp"
#include "screenshot.hpp"
#include "tables.hpp"
#include "trace.hpp"

#include <criware/cpk.hpp>
#include <scarlett/asb.hpp>

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <imgui.h>
#include <span>
#include <stdexcept>
#include <unordered_map>

namespace {

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
					for (const auto& run : sd.tracks[track_index].runs) {
						long len = 0;
						for (uint32_t i = 0; i < run.s8_count && run.s8_start + i < sd.v77.s8.size(); ++i)
							len += sd.v77.s8[run.s8_start + i].frames;
						n = std::max(n, len);
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
				if (const auto* t = sd.find_track(name))
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

DramaPlayback::DramaPlayback(const AssetLoader& loader, DramaPlaybackConfig cfg)
	: _loader(loader)
	, _cfg(std::move(cfg))
	, _renderer(loader)
	, _effects(loader)
	, _camera(1.0f)
	, _projection(glm::ortho(-960.0f, 960.0f, 540.0f, -540.0f)) {
	if (const char* o = std::getenv("VIEWER_ORTHO")) {
		float a = 0, b = 0, c = 0, d = 0;
		if (std::sscanf(o, "%f,%f,%f,%f", &a, &b, &c, &d) == 4)
			_projection = glm::ortho(a, b, c, d);
	}

	const CPKTable& cpk = _loader.cpk();
	const auto slash = _cfg.entry.find_last_of('/');
	const std::string dir = slash == std::string::npos ? "" : _cfg.entry.substr(0, slash);
	const std::string file = slash == std::string::npos ? _cfg.entry : _cfg.entry.substr(slash + 1);
	auto entry = cpk.find_file(dir, file);
	if (entry == cpk.end())
		throw std::runtime_error("script not found in CPK: " + _cfg.entry);

	std::vector<char> bytes;
	cpk.extract(*entry, bytes);
	asb::Module mod =
		asb::parse(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()));
	if (!mod.error.empty())
		throw std::runtime_error("failed to decode " + _cfg.entry + ": " + mod.error);

	asb::CharTable chars;
	if (!_cfg.chara_table.empty())
		chars = asb::load_char_table(_cfg.chara_table);
	asb::CharaSetTable chara_sets = asb::load_chara_set(
		viewer_data_dir() + "/charaset.tsv",
		viewer_data_dir() + "/classes.tsv",
		viewer_data_dir() + "/charanpc_assets.tsv");

	_host.set_call_log([](const std::string&) {});
	if (!chars.empty())
		_host.set_char_table(chars);
	_host.set_chara_set_table(chara_sets);
	_host.set_track_frames([this](const std::string& mbs, int idx) { return mbs_track_frames(_loader, mbs, idx); });
	_host.set_resolve_track(
		[this](const std::string& mbs, const std::string& name) { return mbs_resolve_track(_loader, mbs, name); });
	_host.register_module(mod);
	if (!_host.load(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size())))
		throw std::runtime_error("failed to load " + _cfg.entry);

	asb::Scene scene = asb::extract_scene(mod);
	const asb::SceneFunction* picked = asb::pick_scene_function(scene, _cfg.func);
	if (!picked)
		throw std::runtime_error("no scene function in " + _cfg.entry);
	asIScriptFunction* fn = _host.find_function({picked->name});
	if (!fn)
		throw std::runtime_error("scene function not found: " + picked->name);

	_session = std::make_unique<asb::RuntimeSession>(_host, _cfg.entry);
	if (!_session->start(fn))
		throw std::runtime_error("could not start " + _cfg.entry + ": " + _session->error());

	viewer::trace::stamp("DramaPlayback: started %s::%s", _cfg.entry.c_str(), picked->name.c_str());
	_renderer.sync(_session->scene());
	_effects.update(_session->scene().frame, _session->scene().effects, [this](const std::string& name) -> glm::vec2 {
		if (const auto* e = _session->scene().find_entity(name))
			return {e->pos[0], e->pos[1]};
		return {0.0f, 0.0f};
	});
	_started = true;
}

DramaPlayback::~DramaPlayback() = default;

bool DramaPlayback::handle_inputs() { return _quit; }

void DramaPlayback::step() {
	if (!_session || _session->finished())
		return;
	const auto result = _session->advance();
	if (result == asb::RuntimeSession::StepResult::Error) {
		fprintf(stderr, "[drama] %s\n", _session->error().c_str());
		_quit = true;
		return;
	}
	_frame = _session->frame();
	_renderer.sync(_session->scene());
	_effects.update(_session->scene().frame, _session->scene().effects, [this](const std::string& name) -> glm::vec2 {
		if (const auto* e = _session->scene().find_entity(name))
			return {e->pos[0], e->pos[1]};
		return {0.0f, 0.0f};
	});
	if (result != asb::RuntimeSession::StepResult::Limit) {
		_renderer.advance(1);
		_effects.tick(1.0f / 60.0f);
	}
}

void DramaPlayback::update(float dt) {
	if (_cfg.scene_frame >= 0) {
		if (!_stepped) {
			while (_frame < _cfg.scene_frame && !_session->finished())
				step();
			_stepped = true;
		}
		return;
	}
	if (!_started)
		return;
	_accum += static_cast<double>(dt) * std::max(1.0f, _cfg.fps);
	while (_accum >= 1.0 && !_session->finished()) {
		_accum -= 1.0;
		step();
	}
}

void DramaPlayback::apply_camera() {
	const auto& cam = _session->scene().camera;
	if (cam.has_pos)
		_camera.recenter({cam.pos[0], cam.pos[1]});
	if (cam.has_zoom && cam.zoom > 0.0f)
		_camera.set_zoom(cam.zoom);
}

void DramaPlayback::draw_text() {
	const auto& scene = _session->scene();
	if (scene.texts.empty())
		return;
	auto* draw = ImGui::GetForegroundDrawList();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	float y = viewport->WorkPos.y + viewport->WorkSize.y - 140.0f;
	for (const auto& text : scene.texts) {
		if (scene.frame < text.frame)
			continue;
		if (text.end > 0 && scene.frame >= text.end)
			continue;
		if (text.text.empty())
			continue;
		std::string line;
		if (!text.speaker.empty())
			line = text.speaker + ": ";
		line += text.text;
		draw->AddText(ImVec2(viewport->WorkPos.x + 40.0f, y), IM_COL32(255, 255, 255, 255), line.c_str());
		y += 28.0f;
	}
}

void DramaPlayback::capture() {
	if (_cfg.screenshot.empty() || _captured || _render_count < _cfg.frames)
		return;
	int fb_w = 0, fb_h = 0;
	SDL_GetWindowSizeInPixels(SDL_GL_GetCurrentWindow(), &fb_w, &fb_h);
	if (fb_w <= 0 || fb_h <= 0)
		return;
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
	_captured = true;
}

void DramaPlayback::render() {
	++_render_count;
	if (!_session)
		return;
	const auto& scene = _session->scene();
	apply_camera();
	_renderer.render(_projection, _camera);
	_effects.draw(_projection, _camera, {0.0f, 0.0f});

	float fade[4];
	asb::fade_at(scene.fades, scene.frame, fade);
	_fade.draw(glm::vec4(fade[0], fade[1], fade[2], fade[3]));
	draw_text();
}
