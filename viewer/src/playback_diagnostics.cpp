#include "playback_diagnostics.hpp"

#include "scene_renderer.hpp"
#include "screenshot.hpp"

#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

PlaybackDiagnostics::PlaybackDiagnostics(const ViewerConfig& config)
	: _cfg(config)
	, _capture_mode(!config.capture_frames.empty()) {
	if (!_cfg.trace.empty()) {
		_trace.open(_cfg.trace, _cfg.trace_categories);
		_trace.set_filter(_cfg.trace_frames, _cfg.trace_objects, _cfg.trace_limit);
	}
	if (!_cfg.probe_dir.empty())
		_probe.open(_cfg.probe_dir, _cfg.probe_frames);
	if (!_cfg.record.empty()) {
		if (_cfg.record == "-") {
			_record_out = &std::cout;
		} else {
			const auto parent = std::filesystem::path(_cfg.record).parent_path();
			if (!parent.empty())
				std::filesystem::create_directories(parent);
			_record_file.open(_cfg.record, std::ios::binary | std::ios::trunc);
			if (!_record_file)
				throw std::runtime_error("cannot write record: " + _cfg.record);
			_record_out = &_record_file;
		}
	}
}

PlaybackDiagnostics::~PlaybackDiagnostics() = default;

asb::TraceCallback PlaybackDiagnostics::trace_callback() {
	return [this](const asb::TraceEvent& event) {
		if (event.kind == "vm" && event.event == "call")
			return;
		if (_trace.enabled(event.kind))
			_trace.write(event);
	};
}

void PlaybackDiagnostics::observe_native(const asb::NativeCall& call) {
	_census.observe(call);
	_trace.write_native_call(call);
}

void PlaybackDiagnostics::write_calls(
	const asb::Module& module,
	std::string_view entry,
	std::string_view function) {
	if (_cfg.calls.empty() && _cfg.calls_json.empty())
		return;
	auto emit = [](const std::string& path, const std::string& text) {
		if (path == "-") {
			std::cout << text;
			if (!text.empty() && text.back() != '\n')
				std::cout << '\n';
			return;
		}
		const auto parent = std::filesystem::path(path).parent_path();
		if (!parent.empty())
			std::filesystem::create_directories(parent);
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out) {
			spdlog::error("calls: cannot write {}", path);
			return;
		}
		out << text;
	};
	if (!_cfg.calls.empty())
		emit(_cfg.calls, asb::diag::call_census_text(module, _census, std::string(entry), std::string(function)));
	if (!_cfg.calls_json.empty())
		emit(
			_cfg.calls_json,
			asb::diag::call_census_json(module, _census, std::string(entry), std::string(function)) + "\n");
}

void PlaybackDiagnostics::write_record(const asb::SceneState& scene, const SceneRenderer& renderer, long frame) {
	if (!_record_out)
		return;
	float fade[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	asb::fade_at(scene.fades, scene.frame, fade);
	*_record_out << "{\"frame\":" << frame << ",\"camera\":{\"pos\":" << asb::trace::vec3(scene.camera.pos)
				<< ",\"zoom\":" << asb::trace::number(scene.camera.zoom) << "},\"fade\":[" << asb::trace::number(fade[0])
				<< "," << asb::trace::number(fade[1]) << "," << asb::trace::number(fade[2]) << ","
				<< asb::trace::number(fade[3]) << "],\"stage\":{\"type\":" << scene.stage.stage_type
				<< ",\"weather\":" << scene.stage.weather_type << ",\"coa\":" << scene.stage.coa_preset
				<< ",\"flag\":" << scene.stage.flag_color << "},\"renderer\":" << renderer.timeline_json(scene)
				<< "}\n";
}

long PlaybackDiagnostics::capture_target() const {
	if (!_capture_mode || _capture_index >= _cfg.capture_frames.size())
		return -1;
	return _cfg.capture_frames[_capture_index];
}

void PlaybackDiagnostics::capture(const asb::SceneState& scene, uint32_t render_count) {
	if (_capture_mode) {
		const long frame = scene.frame;
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
	if (_cfg.screenshot.empty() || _captured || render_count < _cfg.frames)
		return;
	if (write_screenshot(_cfg.screenshot))
		_captured = true;
}

bool PlaybackDiagnostics::write_screenshot(const std::string& path) {
	int width = 0, height = 0;
	SDL_GetWindowSizeInPixels(SDL_GL_GetCurrentWindow(), &width, &height);
	if (width <= 0 || height <= 0)
		return false;
	std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	const bool ok = screenshot::write(path, width, height, pixels);
	if (ok)
		spdlog::info("screenshot written: {} ({}x{})", path, width, height);
	else
		spdlog::error("screenshot FAILED: {} ({}x{})", path, width, height);
	return ok;
}

bool PlaybackDiagnostics::probe_mode() const {
	return !_cfg.probe_dir.empty() && !_cfg.probe_frames.empty() && !_capture_mode;
}

long PlaybackDiagnostics::probe_target() {
	while (_probe_index < _cfg.probe_frames.size() && _probe.written(_cfg.probe_frames[_probe_index]))
		++_probe_index;
	if (_probe_index >= _cfg.probe_frames.size())
		return -1;
	return _cfg.probe_frames[_probe_index];
}

void PlaybackDiagnostics::finish_probe(const asb::SceneState& scene, const SceneRenderer& renderer) {
	const std::string json = "{\"frame\":" + std::to_string(scene.frame) + ",\"scene\":" +
							 scene_state_json(scene) + ",\"renderer\":" + renderer.state_json(scene) + "}";
	_probe.write_state(scene.frame, json);
	_probe.write_summary(scene.frame, renderer.summary(scene));
	_probe.end();
}
