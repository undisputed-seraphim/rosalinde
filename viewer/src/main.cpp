#include "engine/Engine.hpp"
#include "scene.hpp"
#include "trace.hpp"

#include <boost/program_options.hpp>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) try {
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
		"class",
		::boost::program_options::value<std::string>(&config.left_class)->default_value(config.left_class),
		"Left character class (screenshot mode)")(
		"class2",
		::boost::program_options::value<std::string>(&config.right_class)->default_value(config.right_class),
		"Right character class (screenshot mode)")(
		"bg",
		::boost::program_options::value<std::string>(&config.background)->default_value(config.background),
		"Battle background/stage name (screenshot mode)")(
		"script",
		::boost::program_options::value<std::string>(&config.script),
		"Render a script-composed scene from a CPK script (e.g. Script/Dr_OW_0_Opening.asb)")(
		"scene-func",
		::boost::program_options::value<std::string>(&config.scene_func),
		"Scene function to build (default: scene00, then MAIN)")(
		"runtime",
		::boost::program_options::bool_switch(&config.runtime),
		"Reconstruct the scene by running the hosted AngelScript VM (playback)")(
		"scene-frame",
		::boost::program_options::value<long>(&config.scene_frame)->default_value(config.scene_frame),
		"Runtime: advance the scene to this frame, then hold (-1 = real time)")(
		"scene-fps",
		::boost::program_options::value<float>(&config.scene_fps)->default_value(config.scene_fps),
		"Runtime: frame step rate (60 matches the opening cinematic)")(
		"scene-limit",
		::boost::program_options::value<int>(&config.scene_limit)->default_value(config.scene_limit),
		"Maximum sprite entities to load (0 = per-mode default)")(
		"scene-camera",
		::boost::program_options::value<std::string>(&config.scene_camera)->default_value(config.scene_camera),
		"Script scene camera: fit (default) | first | last | <index into the scene's camera keys>")(
		"chara-table",
		::boost::program_options::value<std::string>(&config.chara_table),
		"Generated character name -> asset table (see: asb_harvest --chara-table)");
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
} catch (const std::exception& e) {
	std::cout << e.what() << std::endl;
	return 1;
}
