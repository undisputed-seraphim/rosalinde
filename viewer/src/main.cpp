#include "scene.hpp"
#include "engine/Engine.hpp"

#include <boost/program_options.hpp>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
namespace po = ::boost::program_options;

int main(int argc, char* argv[]) try {
	fs::path cpkpath;
	std::string screenshot_path;
	bool debug = false;
	int index = 0;
	std::string classname = "HighPriestess";
	std::string charaname = "Scarlett";
	std::string classname2 = "Crusader";
	std::string charaname2 = "Virginia";
	po::options_description desc;
	desc.add_options()
		("help,h", "Print this help message")
		("cpk", po::value<fs::path>(&cpkpath)->required(), "Path to Unicorn.cpk")
		("class", po::value<std::string>(&classname), "Classname")
		("chara", po::value<std::string>(&charaname), "Character name")
		("class2", po::value<std::string>(&classname2), "Second classname")
		("chara2", po::value<std::string>(&charaname2), "Second character name")
		("dbg,d", po::value<bool>(&debug), "Debug messages in OpenGL")
		("index,i", po::value<int>(&index), "Multipurpose index")
		("screenshot", po::value<std::string>(&screenshot_path), "Take screenshot and exit, saving to given path");
	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	try {
		po::notify(vm);
		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return 1;
		}
	} catch (const po::required_option& e) {
		std::cout << desc << '\n';
		throw;
	}

	uvw::Engine("Rosalinde").run([&] {
		return std::make_unique<Scene>(cpkpath, classname, charaname, classname2, charaname2, index, debug, std::move(screenshot_path));
	});
} catch (const std::exception& e) {
	std::cout << e.what() << std::endl;
	return 1;
}
