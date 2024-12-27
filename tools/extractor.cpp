#include <eltolinde.hpp>

#include <boost/program_options.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <spanstream>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace fs = std::filesystem;
namespace po = boost::program_options;

int main(int argc, char* argv[]) try {
#ifdef _MSC_VER
	SetConsoleOutputCP(CP_UTF8);
#endif
	std::string path_str;
	std::string extract_file;
	bool list = false;
	po::options_description desc;
	desc.add_options()(
		"cpk", po::value<std::string>(&path_str)->required(), "Path to Unicorn.cpk")(
		"extract,e", po::value<std::string>(&extract_file), "Extract file")(
		"list,l", po::value<bool>(&list), "List files");
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

	fs::path file_path(path_str);
	if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
		std::cout << "File at " << file_path << " does not exist." << std::endl;
		return 1;
	}
	std::cout << "Reading file at " << file_path << std::endl;
	if (file_path.extension().string() == ".acb") {
		std::cout << "Got acb file." << std::endl;
		auto ifs = std::ifstream(file_path, std::ios::binary);
		auto acb = ACB(ifs);
		return 0;
	}
	CPK cpk(path_str);

	if (list) {
		for (const auto& file : cpk) {
			std::cout << file.path().string() << "\toffset: " << file.offset
					  << "\tsize: " << file.compressed_size << '\n';
		}
		std::cout << "Discovered " << cpk.size() << " files." << std::endl;
		return 0;
	}

	if (!extract_file.empty()) {
		if (auto entryiter = cpk.by_name(extract_file); entryiter != cpk.end()) {
			std::vector<char> buffer(entryiter->decompressed_size, 0);
			if (cpk.extract(*entryiter, buffer)) {
				//
			} else {
				std::cout << "Failed extraction " << extract_file << '\n';
				return 1;
			}
			auto iss = std::ispanstream(buffer, std::ios::binary);
			if (extract_file.ends_with(".acb")) {
				ACB acb(iss); // Too spammy
			} else if (extract_file.ends_with(".ftx")) {
				const auto textures = FTX::parse(iss);
				for (const auto& entry : textures) {
					std::cout << entry.name << " w: " << entry.width << " h: " << entry.height << std::endl;
					const char* data = reinterpret_cast<const char*>(entry.rgba.data());
					std::ofstream(entry.name, std::ios::binary).write(data, entry.rgba.size());
				}
			} else if (extract_file.ends_with(".fms")) {
				const auto lines = parse_fms(iss);
				for (const auto& l : lines) {
					if (l.empty()) continue;
					std::cout << l << '\n';
				}
			} else if (extract_file.ends_with(".mbs")) {
				MBS mbs(iss);
				std::cout << mbs.filename() << std::endl;
				auto q = mbs.extract();
			}
		} else {
			std::cout << extract_file << " not found.\n";
			return 0;
		}
	}

	return 0;
} catch (const std::exception& e) {
	std::cerr << e.what() << std::endl;
	return 1;
}