#include "engine/Engine.hpp"
#include "scene.hpp"

#include <boost/program_options.hpp>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) try {
	std::filesystem::path cpkpath;
	::boost::program_options::options_description desc;
	desc.add_options()("help,h", "Print this help message")(
		"cpk", ::boost::program_options::value<std::filesystem::path>(&cpkpath)->required(), "Path to Unicorn.cpk");
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

	uvw::Engine("Rosalinde").run([&] { return std::make_unique<Scene>(cpkpath); });
} catch (const std::exception& e) {
	std::cout << e.what() << std::endl;
	return 1;
}
