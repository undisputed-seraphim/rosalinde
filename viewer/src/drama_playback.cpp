#include "drama_playback.hpp"
#include "playback_clock.hpp"
#include "trace.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

DramaPlayback::DramaPlayback(const AssetLoader& loader, ViewerConfig config)
	: _loader(loader)
	, _cfg(std::move(config))
	, _library(loader) {
	if (!_cfg.capture_frames.empty()) {
		std::sort(_cfg.capture_frames.begin(), _cfg.capture_frames.end());
		_cfg.capture_frames.erase(
			std::unique(_cfg.capture_frames.begin(), _cfg.capture_frames.end()), _cfg.capture_frames.end());
		_cfg.scene_frame = std::max(_cfg.scene_frame, _cfg.capture_frames.back());
	}
	if (!_cfg.probe_dir.empty() && !_cfg.probe_frames.empty()) {
		std::sort(_cfg.probe_frames.begin(), _cfg.probe_frames.end());
		_cfg.probe_frames.erase(
			std::unique(_cfg.probe_frames.begin(), _cfg.probe_frames.end()), _cfg.probe_frames.end());
		_cfg.scene_frame = std::max(_cfg.scene_frame, _cfg.probe_frames.back());
	}

	_diagnostics = std::make_unique<PlaybackDiagnostics>(_cfg);
	const auto trace_cb = _diagnostics->trace_callback();
	PlaybackRuntime::NativeCallback native_cb;
	if (_diagnostics->wants_native_calls())
		native_cb = [this](const asb::NativeCall& call) { _diagnostics->observe_native(call); };
	_runtime = std::make_unique<PlaybackRuntime>(_loader, _library.mbs_catalog(), _cfg, trace_cb, std::move(native_cb));
	_view = std::make_unique<SceneView>(_loader, _library, _cfg);
	_view->set_trace(trace_cb);
	_view->set_filters(_cfg.only, _cfg.hide, _cfg.keep_stage);
	_view->initialize(_runtime->scene());
	_started = true;
	_diagnostics->write_record(_runtime->scene(), _view->renderer(), 0);
	viewer::trace::stamp("DramaPlayback: started %s::%s", _cfg.script.c_str(), _runtime->entry_name().c_str());
}

DramaPlayback::~DramaPlayback() {
	if (_runtime && _diagnostics)
		_diagnostics->write_calls(_runtime->module(), _cfg.script, _runtime->entry_name());
}

bool DramaPlayback::handle_inputs() { return _quit; }

void DramaPlayback::step() {
	if (!_runtime || _runtime->finished())
		return;
	const long trace_frame = static_cast<long>(_runtime->scene().frame);
	const auto result = _runtime->advance();
	if (result == asb::RuntimeSession::StepResult::Error) {
		spdlog::error("drama: {}", _runtime->error());
		_quit = true;
		return;
	}
	_frame = _runtime->frame();
	spdlog::trace(
		"step: frame={} entities={} stage={} cam=({:.1f},{:.1f}) zoom={:.3f}",
		trace_frame,
		_runtime->scene().entities.size(),
		_runtime->scene().stage.stage_type,
		_runtime->scene().camera.pos[0],
		_runtime->scene().camera.pos[1],
		_runtime->scene().camera.zoom);
	if ((trace_frame % 60) == 0)
		spdlog::debug(
			"play: frame={} entities={} stage={} cam=({:.1f},{:.1f}) zoom={:.3f}",
			trace_frame,
			_runtime->scene().entities.size(),
			_runtime->scene().stage.stage_type,
			_runtime->scene().camera.pos[0],
			_runtime->scene().camera.pos[1],
			_runtime->scene().camera.zoom);

	const auto& scene = _runtime->scene();
	_view->sync(scene);
	if (result != asb::RuntimeSession::StepResult::Limit)
		_view->advance(PlaybackClock::frame_seconds);

	_diagnostics->write_event(
		{_frame,
		 "timeline",
		 "step",
		 "\"entities\":" + std::to_string(scene.entities.size()) + ",\"stage\":" +
			 std::to_string(scene.stage.stage_type) + ",\"camera_pos\":" + asb::trace::vec3(scene.camera.pos) +
			 ",\"camera_zoom\":" + asb::trace::number(scene.camera.zoom)});
	if (_frame % _cfg.record_every == 0)
		_diagnostics->write_record(scene, _view->renderer(), _frame);
}

void DramaPlayback::update(float dt) {
	if (!_runtime || !_started)
		return;
	if (_diagnostics->capture_mode()) {
		const long target = _diagnostics->capture_target();
		if (target < 0)
			return;
		while (_frame < target && !_runtime->finished())
			step();
		if (_runtime->finished() && _frame < target) {
			spdlog::warn("capture: scene ended at frame {} before requested frame {}", _frame, target);
			_diagnostics->finish_captures();
		}
		return;
	}
	if (_diagnostics->probe_mode()) {
		const long target = _diagnostics->probe_target();
		if (target < 0)
			return;
		while (_frame < target && !_runtime->finished())
			step();
		return;
	}
	if (_cfg.scene_frame >= 0) {
		if (!_stepped) {
			while (_frame < _cfg.scene_frame && !_runtime->finished())
				step();
			_stepped = true;
		}
		return;
	}
	_accum += static_cast<double>(dt) * std::max(1.0f, _cfg.scene_fps);
	while (_accum >= 1.0 && !_runtime->finished()) {
		_accum -= 1.0;
		step();
	}
}

void DramaPlayback::render() {
	++_render_count;
	if (!_runtime)
		return;
	const auto& scene = _runtime->scene();
	const long frame = static_cast<long>(scene.frame);
	const bool probe_now = _diagnostics->begin_probe(frame);
	_view->render(scene, probe_now ? _diagnostics->probe() : nullptr);
	if (probe_now)
		_diagnostics->finish_probe(scene, _view->renderer());
}

void DramaPlayback::capture() {
	if (_runtime)
		_diagnostics->capture(_runtime->scene(), _render_count);
}

bool DramaPlayback::is_done() const { return _runtime && _runtime->finished(); }

bool DramaPlayback::probes_done() const { return _diagnostics && _diagnostics->probes_done(); }

bool DramaPlayback::captured() const { return _diagnostics && _diagnostics->captured(); }

bool DramaPlayback::captures_done() const { return _diagnostics && _diagnostics->captures_done(); }
