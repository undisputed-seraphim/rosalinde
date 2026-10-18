#include "drama_playback.hpp"
#include "screenshot.hpp"
#include "tables.hpp"
#include "trace.hpp"

#include <spdlog/spdlog.h>

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
			const cpk::archive& cpk = loader.cpk();
			if (auto e = cpk.find(p->dir, p->path); e != cpk.end()) {
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
			const cpk::archive& cpk = loader.cpk();
			if (auto e = cpk.find(p->dir, p->path); e != cpk.end()) {
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

std::string stage_background_name(int type) {
	switch (type) {
	case 179:
	case 180:
		return "BGOpening010";
	case 82:
	case 83:
	case 78:
		return "BGOpening090";
	default:
		return {};
	}
}

} // namespace

DramaPlayback::DramaPlayback(const AssetLoader& loader, DramaPlaybackConfig cfg)
	: _loader(loader)
	, _cfg(std::move(cfg))
	, _renderer(loader)
	, _effects(loader)
	, _camera(1.0f)
	, _projection(glm::ortho(-960.0f, 960.0f, 540.0f, -540.0f)) {
	if (!_cfg.capture_frames.empty()) {
		_capture_mode = true;
		std::sort(_cfg.capture_frames.begin(), _cfg.capture_frames.end());
		_cfg.capture_frames.erase(
			std::unique(_cfg.capture_frames.begin(), _cfg.capture_frames.end()), _cfg.capture_frames.end());
		const long last = _cfg.capture_frames.back();
		_cfg.scene_frame = std::max(_cfg.scene_frame, last);
	}
	if (!_cfg.probe_dir.empty() && !_cfg.probe_frames.empty()) {
		std::sort(_cfg.probe_frames.begin(), _cfg.probe_frames.end());
		_cfg.probe_frames.erase(
			std::unique(_cfg.probe_frames.begin(), _cfg.probe_frames.end()), _cfg.probe_frames.end());
		_cfg.scene_frame = std::max(_cfg.scene_frame, _cfg.probe_frames.back());
		_probe_mode = !_capture_mode;
	}
	if (!_cfg.trace.empty())
		_trace.open(_cfg.trace, _cfg.trace_categories);
	if (!_cfg.probe_dir.empty())
		_probe.open(_cfg.probe_dir, _cfg.probe_frames);
	auto trace_cb = [this](const asb::TraceEvent& e) {
		if (e.kind == "vm" && e.event == "call")
			return;
		if (_trace.enabled(e.kind))
			_trace.write(e);
	};
	_host.set_trace(trace_cb);
	_host.set_native_call([this](const asb::NativeCall& call) { _trace.write_native_call(call); });
	_renderer.set_trace(trace_cb);
	_renderer.set_filters(_cfg.only, _cfg.hide, _cfg.keep_stage);
	if (const char* o = std::getenv("VIEWER_ORTHO")) {
		float a = 0, b = 0, c = 0, d = 0;
		if (std::sscanf(o, "%f,%f,%f,%f", &a, &b, &c, &d) == 4)
			_projection = glm::ortho(a, b, c, d);
	}

	const cpk::archive& cpk = _loader.cpk();
	const auto slash = _cfg.entry.find_last_of('/');
	const std::string dir = slash == std::string::npos ? "" : _cfg.entry.substr(0, slash);
	const std::string file = slash == std::string::npos ? _cfg.entry : _cfg.entry.substr(slash + 1);
	auto entry = cpk.find(dir, file);
	if (entry == cpk.end())
		throw std::runtime_error("script not found in CPK: " + _cfg.entry);

	std::vector<char> bytes;
	cpk.extract(*entry, bytes);
	asb::Module mod =
		asb::parse(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()));
	if (!mod.error.empty())
		throw std::runtime_error("failed to decode " + _cfg.entry + ": " + mod.error);

	const std::string data_dir = _cfg.data_dir.empty() ? viewer_data_dir() : _cfg.data_dir;
	_runtime = load_viewer_runtime_data(_loader.cpk(), _cfg.chara_table, data_dir);

	_host.set_call_log([](const std::string&) {});
	configure_host(_host, _runtime);
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
	sync_stage();
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
	const long trace_frame = static_cast<long>(_session->scene().frame);
	const auto result = _session->advance();
	if (result == asb::RuntimeSession::StepResult::Error) {
		spdlog::error("drama: {}", _session->error());
		_quit = true;
		return;
	}
	_frame = _session->frame();
	if ((trace_frame % 60) == 0)
		spdlog::debug(
			"play: frame={} entities={} stage={} cam=({:.1f},{:.1f}) zoom={:.3f}",
			trace_frame,
			_session->scene().entities.size(),
			_session->scene().stage.stage_type,
			_session->scene().camera.pos[0],
			_session->scene().camera.pos[1],
			_session->scene().camera.zoom);
	sync_stage();
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
		if (_capture_mode) {
			if (_capture_index >= _cfg.capture_frames.size())
				return;
			const long target = _cfg.capture_frames[_capture_index];
			while (_frame < target && !_session->finished())
				step();
			if (_session->finished() && _frame < target) {
				spdlog::warn("capture: scene ended at frame {} before requested frame {}", _frame, target);
				_captures_done = true;
			}
			return;
		}
		if (_probe_mode) {
			while (_probe_index < _cfg.probe_frames.size() && _probe.written(_cfg.probe_frames[_probe_index]))
				++_probe_index;
			if (_probe_index >= _cfg.probe_frames.size())
				return;
			const long target = _cfg.probe_frames[_probe_index];
			while (_frame < target && !_session->finished())
				step();
			return;
		}
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

void DramaPlayback::sync_stage() {
	const int type = _session->scene().stage.stage_type;
	if (type == _stage_type)
		return;
	_stage_type = type;
	const std::string name = stage_background_name(type);
	spdlog::debug("stage: frame={} type={} -> {}", _session->scene().frame, type, name);
	auto it = BattleBGs.find(name);
	if (it == BattleBGs.end()) {
		_renderer.set_stage({}, {});
		return;
	}
	const std::string mbs =
		it->second.mbs.dir.empty() ? it->second.mbs.path : it->second.mbs.dir + "/" + it->second.mbs.path;
	const std::string ftx =
		it->second.ftx.dir.empty() ? it->second.ftx.path : it->second.ftx.dir + "/" + it->second.ftx.path;
	_renderer.set_stage(mbs, ftx);
	_trace.write(
		{static_cast<long>(_session->scene().frame),
		 "stage",
		 "asset",
		 "\"type\":" + asb::trace::integer(type) + ",\"mbs\":\"" + asb::trace::quote(mbs) + "\",\"ftx\":\"" +
			 asb::trace::quote(ftx) + "\""});
}

void DramaPlayback::apply_camera() {
	const auto& cam = _session->scene().camera;
	const float zoom = cam.has_zoom && cam.zoom > 0.0f ? cam.zoom : 1.0f;
	if (cam.has_pos) {
		float oy = 0.0f;
		if (const char* e = std::getenv("VIEWER_CAM_Y"))
			oy = static_cast<float>(std::atof(e));
		_camera.recenter({cam.pos[0], cam.pos[1] + oy});
	}
	if (cam.has_zoom && cam.zoom > 0.0f)
		_camera.set_zoom(cam.zoom);
	_trace.write(
		{static_cast<long>(_session->scene().frame),
		 "camera",
		 "eval",
		 "\"pos\":" + asb::trace::vec3(cam.pos) + ",\"zoom\":" + asb::trace::number(zoom) +
			 ",\"origin\":" + asb::trace::vec3(cam.origin)});
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

bool DramaPlayback::write_screenshot(const std::string& path) {
	int fb_w = 0, fb_h = 0;
	SDL_GetWindowSizeInPixels(SDL_GL_GetCurrentWindow(), &fb_w, &fb_h);
	if (fb_w <= 0 || fb_h <= 0)
		return false;
	std::vector<uint8_t> pixels(static_cast<size_t>(fb_w) * fb_h * 4);
	glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	const bool ok = screenshot::write(path, fb_w, fb_h, pixels);
	if (ok)
		spdlog::info("screenshot written: {} ({}x{})", path, fb_w, fb_h);
	else
		spdlog::error("screenshot FAILED: {} ({}x{})", path, fb_w, fb_h);
	return ok;
}

void DramaPlayback::capture() {
	if (_capture_mode) {
		const long frame = _session ? _session->scene().frame : _frame;
		const bool wanted =
			std::find(_cfg.capture_frames.begin(), _cfg.capture_frames.end(), frame) != _cfg.capture_frames.end();
		if (!wanted || _captured_frames.contains(frame))
			return;
		std::filesystem::create_directories(_cfg.capture_dir);
		char name[32];
		std::snprintf(name, sizeof(name), "%s-%06ld.png", _cfg.capture_prefix.c_str(), frame);
		const auto path = std::filesystem::path(_cfg.capture_dir) / name;
		if (write_screenshot(path.string())) {
			_captured_frames.insert(frame);
			while (_capture_index < _cfg.capture_frames.size() &&
				   _captured_frames.contains(_cfg.capture_frames[_capture_index]))
				++_capture_index;
			_captures_done = _capture_index >= _cfg.capture_frames.size();
		}
		return;
	}
	if (_cfg.screenshot.empty() || _captured || _render_count < _cfg.frames)
		return;
	if (write_screenshot(_cfg.screenshot))
		_captured = true;
}

void DramaPlayback::render() {
	++_render_count;
	if (!_session)
		return;
	const auto& scene = _session->scene();
	const long frame = static_cast<long>(scene.frame);
	const bool probe_now = _probe.begin(frame);
	_renderer.set_probe(probe_now ? &_probe : nullptr);
	apply_camera();
	_renderer.render(_projection, _camera);
	_effects.draw(_projection, _camera, {0.0f, 0.0f});

	float fade[4];
	asb::fade_at(scene.fades, scene.frame, fade);
	_fade.draw(glm::vec4(fade[0], fade[1], fade[2], fade[3]));
	draw_text();

	if (probe_now) {
		const std::string json = "{\"frame\":" + std::to_string(frame) + ",\"scene\":" + scene_state_json(scene) +
								 ",\"renderer\":" + _renderer.state_json(scene) + "}";
		_probe.write_state(frame, json);
		_probe.write_summary(frame, _renderer.summary(scene));
		_renderer.set_probe(nullptr);
		_probe.end();
	}
}
