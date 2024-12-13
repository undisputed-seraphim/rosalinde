#include <argparse.hpp>
#include <eltolinde.hpp>

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

int main(int argc, char* argv[]) try {
#ifdef _MSC_VER
	SetConsoleOutputCP(CP_UTF8);
#endif
	std::string path_str;
	std::string extract_file;
	argparse::ArgumentParser program("rosalinde");
	program.add_argument("--cpk").required().help("Input CPK file to examine.").store_into(path_str);
	program.add_argument("--extract", "-e").help("Extract file by name.").store_into(extract_file);
	program.add_argument("--list", "-l").flag().help("List files within the CPK archive.");

	if (argc == 1) {
		std::cout << program << std::endl;
		return 0;
	}
	try {
		program.parse_args(argc, argv);
	} catch (const std::exception& e) {
		std::cerr << e.what() << '\n';
		std::cerr << program << std::endl;
		return 1;
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

	if (program["--list"] == true) {
		for (const auto& file : cpk) {
			std::cout << file.path << " / " << file.name << "\toffset: " << file.offset
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
				auto ofs = std::ofstream("dump.txt");
				ofs << q;
				ofs = std::ofstream(mbs.filename(), std::ios::binary);
				ofs.write(buffer.data(), buffer.size());
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