#include "engine/Engine.hpp"
#include "log.hpp"
#include "scene.hpp"
#include "trace.hpp"

#include <algorithm>
#include <array>
#include <boost/program_options.hpp>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<long> parse_frames(const std::string& text) {
	std::vector<long> frames;
	size_t start = 0;
	while (start < text.size()) {
		const size_t comma = text.find(',', start);
		const std::string item = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
		if (!item.empty()) {
			const size_t dash = item.find('-', 1);
			if (dash != std::string::npos) {
				const long first = std::stol(item.substr(0, dash));
				const long last = std::stol(item.substr(dash + 1));
				for (long frame = first; frame <= last; ++frame)
					frames.push_back(frame);
			} else {
				frames.push_back(std::stol(item));
			}
		}
		if (comma == std::string::npos)
			break;
		start = comma + 1;
	}
	return frames;
}

bool parse_color(std::string text, std::array<float, 4>& out) {
	if (!text.empty() && text.front() == '#')
		text.erase(0, 1);
	if (text.size() == 6 || text.size() == 8) {
		const bool hex = std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
		if (hex) {
			auto byte = [&](size_t at) {
				return static_cast<float>(std::stoi(text.substr(at, 2), nullptr, 16)) / 255.0f;
			};
			out = {byte(0), byte(2), byte(4), text.size() == 8 ? byte(6) : 1.0f};
			return true;
		}
	}
	float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
	const int count = std::sscanf(text.c_str(), "%f,%f,%f,%f", &r, &g, &b, &a);
	if (count < 3)
		return false;
	if (count == 3)
		a = 1.0f;
	if (r > 1.0f || g > 1.0f || b > 1.0f) {
		r /= 255.0f;
		g /= 255.0f;
		b /= 255.0f;
	}
	if (a > 1.0f)
		a /= 255.0f;
	out = {r, g, b, a};
	return true;
}

} // namespace

int main(int argc, char* argv[]) try {
	viewer::log::init();
	viewer::trace::reset();
	viewer::trace::stamp("main() enter");
	ViewerConfig config;
	std::string clear_color_text;
	std::string probe_frames_text;
	std::string trace_frames_text;
	::boost::program_options::options_description desc;
	desc.add_options()("help,h", "Print this help message")(
		"cpk", ::boost::program_options::value<std::filesystem::path>(&config.cpk)->required(), "Path to Unicorn.cpk")(
		"screenshot",
		::boost::program_options::value<std::string>(&config.screenshot),
		"Render to an image (.png/.ppm) after --frames frames, then exit")(
		"frames",
		::boost::program_options::value<uint32_t>(&config.frames)->default_value(1),
		"Frames to render before taking the screenshot")(
		"script",
		::boost::program_options::value<std::string>(&config.script),
		"Drama script to play (e.g. Script/Dr_OW_0_Opening.asb)")(
		"scene-func",
		::boost::program_options::value<std::string>(&config.scene_func),
		"Scene entry function (default: auto-pick)")(
		"scene-frame",
		::boost::program_options::value<long>(&config.scene_frame)->default_value(config.scene_frame),
		"Advance to this scene frame, then hold (-1 = real time)")(
		"scene-fps",
		::boost::program_options::value<float>(&config.scene_fps)->default_value(config.scene_fps),
		"Playback frame step rate (60 matches the script clock)")(
		"trace",
		::boost::program_options::value<std::string>(&config.trace),
		"Write structured NDJSON events to this file")(
		"trace-categories",
		::boost::program_options::value<std::string>(&config.trace_categories)->default_value("all"),
		"Comma-separated trace categories: vm,stage,camera,asset,sprite,draw,timeline (default: all)")(
		"trace-frames",
		::boost::program_options::value<std::string>(&trace_frames_text),
		"Only trace these scene frames (comma-separated, ranges allowed)")(
		"trace-object",
		::boost::program_options::value<std::vector<std::string>>(&config.trace_objects)->composing(),
		"Only trace events mentioning this object/call text (repeatable)")(
		"trace-limit",
		::boost::program_options::value<int>(&config.trace_limit)->default_value(0),
		"Maximum trace events (0 = unlimited)")(
		"calls",
		::boost::program_options::value<std::string>(&config.calls),
		"Write native-call census text to this file")(
		"calls-json",
		::boost::program_options::value<std::string>(&config.calls_json),
		"Write native-call census JSON to this file")(
		"record",
		::boost::program_options::value<std::string>(&config.record),
		"Write compact runtime timeline NDJSON to this file")(
		"record-every",
		::boost::program_options::value<long>(&config.record_every)->default_value(1),
		"Record one timeline row every N scene frames")(
		"capture-frames",
		::boost::program_options::value<std::string>(),
		"Capture comma-separated scene frames in one run (e.g. 0,60,480)")(
		"capture-dir",
		::boost::program_options::value<std::string>(&config.capture_dir),
		"Directory for --capture-frames images")(
		"capture-prefix",
		::boost::program_options::value<std::string>(&config.capture_prefix)->default_value("frame"),
		"Filename prefix for --capture-frames images")(
		"chara-table",
		::boost::program_options::value<std::string>(&config.chara_table),
		"Generated character name -> asset table (see: asbrun export chara)")(
		"data-dir",
		::boost::program_options::value<std::string>(&config.data_dir),
		"Runtime data directory (default: viewer/data or $VIEWER_DATA)")(
		"only",
		::boost::program_options::value<std::vector<std::string>>(&config.only)->composing(),
		"Render only objects with this exact name or alias (repeatable)")(
		"hide",
		::boost::program_options::value<std::vector<std::string>>(&config.hide)->composing(),
		"Hide objects whose name or alias contains this text (repeatable)")(
		"keep-stage",
		::boost::program_options::bool_switch(&config.keep_stage),
		"Keep the stage/background when --only is active")(
		"clear-color",
		::boost::program_options::value<std::string>(&clear_color_text),
		"Background clear color: #RRGGBB[AA] or R,G,B[,A]")(
		"probe",
		::boost::program_options::value<std::string>(&config.probe_dir),
		"Write state/draw/summary probe files to this directory")(
		"probe-frames",
		::boost::program_options::value<std::string>(&probe_frames_text),
		"Comma-separated frames for --probe (default: --capture-frames or --scene-frame)");
	::boost::program_options::variables_map vm;
	::boost::program_options::store(::boost::program_options::parse_command_line(argc, argv, desc), vm);
	try {
		::boost::program_options::notify(vm);
		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return 1;
		}
	} catch (const ::boost::program_options::required_option& e) {
		std::cout << desc << '\n';
		throw;
	}
	if (config.script.empty()) {
		spdlog::error("--script is required");
		return 1;
	}
	if (vm.count("capture-frames")) {
		config.capture_frames = parse_frames(vm["capture-frames"].as<std::string>());
		if (config.capture_frames.empty()) {
			spdlog::error("--capture-frames is empty");
			return 1;
		}
		if (config.capture_dir.empty()) {
			spdlog::error("--capture-dir is required with --capture-frames");
			return 1;
		}
	}
	if (!clear_color_text.empty() && !parse_color(clear_color_text, config.clear_color)) {
		spdlog::error("invalid --clear-color: {}", clear_color_text);
		return 1;
	}
	if (!trace_frames_text.empty()) {
		config.trace_frames = parse_frames(trace_frames_text);
		if (config.trace_frames.empty()) {
			spdlog::error("--trace-frames is empty");
			return 1;
		}
		if (std::any_of(config.trace_frames.begin(), config.trace_frames.end(), [](long f) { return f < 0; })) {
			spdlog::error("--trace-frames must be non-negative");
			return 1;
		}
	}
	if (config.trace_limit < 0) {
		spdlog::error("--trace-limit must be non-negative");
		return 1;
	}
	if (config.record_every <= 0) {
		spdlog::error("--record-every must be positive");
		return 1;
	}
	if (!probe_frames_text.empty()) {
		config.probe_frames = parse_frames(probe_frames_text);
		if (config.probe_frames.empty()) {
			spdlog::error("--probe-frames is empty");
			return 1;
		}
		if (std::any_of(config.probe_frames.begin(), config.probe_frames.end(), [](long f) { return f < 0; })) {
			spdlog::error("--probe-frames must be non-negative");
			return 1;
		}
	}
	if (!config.probe_dir.empty() && config.probe_frames.empty()) {
		if (!config.capture_frames.empty())
			config.probe_frames = config.capture_frames;
		else if (config.scene_frame >= 0)
			config.probe_frames = {config.scene_frame};
		else
			config.probe_frames = {0};
	}

	unsigned view_w = 1920, view_h = 1080;
	if (const char* size = std::getenv("VIEWER_SIZE")) {
		unsigned w = 0, h = 0;
		if (std::sscanf(size, "%ux%u", &w, &h) == 2 && w && h) {
			view_w = w;
			view_h = h;
		}
	}

	uvw::Engine engine("Rosalinde", view_w, view_h);
	engine.set_clear_color(config.clear_color);
	engine.run([&] {
		viewer::trace::stamp("creating Scene");
		auto scene = std::make_unique<Scene>(config);
		viewer::trace::stamp("Scene constructed");
		return scene;
	});
	viewer::trace::stamp("Engine::run returned");
	viewer::log::shutdown();
} catch (const std::exception& e) {
	spdlog::error("{}", e.what());
	return 1;
}
