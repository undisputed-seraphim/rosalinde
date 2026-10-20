#pragma once

#include "probe.hpp"
#include "trace_writer.hpp"
#include "viewer_config.hpp"

#include <scarlett/call_census.hpp>
#include <scarlett/scene.hpp>

#include <cstdint>
#include <fstream>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

class SceneRenderer;

class PlaybackDiagnostics {
public:
	explicit PlaybackDiagnostics(const ViewerConfig& config);
	~PlaybackDiagnostics();

	asb::TraceCallback trace_callback();
	void write_event(const asb::TraceEvent& event) { _trace.write(event); }
	void observe_native(const asb::NativeCall& call);
	void write_calls(const asb::Module& module, std::string_view entry, std::string_view function);
	void write_record(const asb::SceneState& scene, const SceneRenderer& renderer, long frame);

	bool capture_mode() const { return _capture_mode; }
	long capture_target() const;
	bool capture_finished() const { return _captures_done; }
	void finish_captures() { _captures_done = true; }
	void capture(const asb::SceneState& scene, uint32_t render_count);
	bool captured() const { return _captured; }
	bool captures_done() const { return _captures_done; }

	bool probe_mode() const;
	long probe_target();
	bool probe_written(long frame) const { return _probe.written(frame); }
	bool begin_probe(long frame) { return _probe.begin(frame); }
	ProbeWriter* probe() { return &_probe; }
	void finish_probe(const asb::SceneState& scene, const SceneRenderer& renderer);
	bool probes_done() const { return _probe.done(); }

private:
	bool write_screenshot(const std::string& path);

	ViewerConfig _cfg;
	TraceWriter _trace;
	ProbeWriter _probe;
	asb::diag::CallCensus _census;
	std::ofstream _record_file;
	std::ostream* _record_out = nullptr;

	bool _capture_mode = false;
	size_t _capture_index = 0;
	std::unordered_set<long> _captured_frames;
	bool _captured = false;
	bool _captures_done = false;
	size_t _probe_index = 0;
};
