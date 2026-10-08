#include "engine/Engine.hpp"
#include "scene.hpp"

#include <boost/program_options.hpp>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) try {
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
		"Right character class (screenshot mode)");
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

	uvw::Engine("Rosalinde").run([&] { return std::make_unique<Scene>(config); });
} catch (const std::exception& e) {
	std::cout << e.what() << std::endl;
	return 1;
}
