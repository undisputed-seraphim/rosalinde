#include "engine/Engine.hpp"
#include "log.hpp"
#include "scene.hpp"
#include "trace.hpp"

#include <boost/program_options.hpp>
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
		if (!item.empty())
			frames.push_back(std::stol(item));
		if (comma == std::string::npos)
			break;
		start = comma + 1;
	}
	return frames;
}

} // namespace

int main(int argc, char* argv[]) try {
	viewer::log::init();
	viewer::trace::reset();
	viewer::trace::stamp("main() enter");
	ViewerConfig config;
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
		"Comma-separated trace categories: vm,stage,camera,asset,sprite,draw (default: all)")(
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
		"Generated character name -> asset table (see: asbrun export chara)");
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

	unsigned view_w = 1920, view_h = 1080;
	if (const char* size = std::getenv("VIEWER_SIZE")) {
		unsigned w = 0, h = 0;
		if (std::sscanf(size, "%ux%u", &w, &h) == 2 && w && h) {
			view_w = w;
			view_h = h;
		}
	}

	uvw::Engine("Rosalinde", view_w, view_h).run([&] {
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
