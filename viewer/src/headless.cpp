#include "headless.hpp"

#include "asset_loader.hpp"
#include "playback_diagnostics.hpp"
#include "playback_runtime.hpp"
#include "probe.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool write_output(const std::string& path, const std::string& text) {
	if (path == "-") {
		std::cout << text;
		if (!text.empty() && text.back() != '\n')
			std::cout << '\n';
		return true;
	}
	const auto parent = std::filesystem::path(path).parent_path();
	if (!parent.empty())
		std::filesystem::create_directories(parent);
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out) {
		spdlog::error("headless: cannot write {}", path);
		return false;
	}
	out << text;
	if (!text.empty() && text.back() != '\n')
		out << '\n';
	return true;
}

} // namespace

int run_headless(const ViewerConfig& config) {
	AssetLoader loader(config.cpk);
	PlaybackDiagnostics diagnostics(config);
	PlaybackRuntime runtime(
		loader,
		config,
		diagnostics.trace_callback(),
		[&diagnostics](const asb::NativeCall& call) { diagnostics.observe_native(call); });
	runtime.set_max_frames(config.max_frames);
	const SceneReportContext report{&runtime.mbs_info()};

	long target = -1;
	if (config.run_to >= 0)
		target = config.run_to;
	else if (config.scene_frame >= 0)
		target = config.scene_frame;
	if (!config.probe_frames.empty()) {
		const long last_probe = *std::max_element(config.probe_frames.begin(), config.probe_frames.end());
		if (target < 0 || last_probe > target)
			target = last_probe;
	}
	if (target < 0)
		target = config.max_frames;
	else if (target > config.max_frames) {
		spdlog::warn("headless: requested frame {} exceeds --max-frames {}; stopping at {}",
			target,
			config.max_frames,
			config.max_frames);
		target = config.max_frames;
	}

	if (diagnostics.probe_wanted(runtime.frame()))
		diagnostics.write_scene_probe(runtime.scene(), report);
	if (!config.record.empty() && runtime.frame() % config.record_every == 0)
		diagnostics.write_scene_record(runtime.scene(), runtime.frame(), report);

	while (!runtime.finished() && runtime.frame() < target) {
		const auto step = runtime.advance();
		if (step == asb::RuntimeSession::StepResult::Error) {
			spdlog::error("headless: {}", runtime.error());
			diagnostics.write_calls(runtime.module(), config.script, runtime.entry_name());
			return 1;
		}
		const long frame = runtime.frame();
		if (!config.record.empty() && frame % config.record_every == 0)
			diagnostics.write_scene_record(runtime.scene(), frame, report);
		if (diagnostics.probe_wanted(frame))
			diagnostics.write_scene_probe(runtime.scene(), report);
		if (step == asb::RuntimeSession::StepResult::Limit)
			break;
	}

	bool ok = true;
	if (!config.state.empty())
		ok &= write_output(config.state, scene_state_text(runtime.scene(), report));
	if (!config.state_json.empty())
		ok &= write_output(config.state_json, scene_state_json(runtime.scene(), report));
	diagnostics.write_calls(runtime.module(), config.script, runtime.entry_name());

	if (runtime.frame() < target)
		spdlog::warn("headless: stopped at frame {} before requested {}", runtime.frame(), target);
	if (!config.probe_frames.empty() && !diagnostics.probes_done())
		spdlog::warn("headless: not all probe frames were reached");
	spdlog::info(
		"headless: {}: frame {}{}",
		config.script,
		runtime.frame(),
		runtime.finished() ? " (finished)" : (runtime.frame() < target ? " (limit)" : " (stopped)"));
	return ok ? 0 : 1;
}
