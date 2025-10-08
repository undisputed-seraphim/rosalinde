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

std::pair<std::string_view, std::string_view> decompose_path(std::string_view extract_file) {
	const auto l = extract_file.find_last_of('/');
	return {extract_file.substr(0, l), extract_file.substr(l + 1)};
}

int main(int argc, char* argv[]) try {
#ifdef _MSC_VER
	SetConsoleOutputCP(CP_UTF8);
#endif
	std::string path_str;
	std::string extract_file;
	bool list = false;
	po::options_description desc;
	// clang-format off
	desc.add_options()(
		"cpk", po::value<std::string>(&path_str)->required(), "Path to Unicorn.cpk")(
		"extract,e", po::value<std::string>(&extract_file), "Extract file")(
		"list,l", po::bool_switch(&list), "List files");
	// clang-format on
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
	auto cpktable = TopLevelCpk(path_str).getTableOfContents();
	if (list) {
		for (const auto& [DirName, FileName, FileSize, ExtractSize, FileOffset, ID] : cpktable) {
			std::cout << ID << ": " << DirName << " / " << FileName << "\t" << ExtractSize << '\n';
		}
		return 0;
	}

	const auto [dir, name] = decompose_path(extract_file);
	std::cout << dir << ' ' << name << '\n';

	if (auto entry = cpktable.find_file(dir, name); entry != cpktable.end()) {
		std::vector<char> buffer;
		cpktable.extract(*entry, buffer);
		auto iss = std::ispanstream(buffer, std::ios::binary);
		if (extract_file.ends_with(".acb")) {
			UTF utf;
			iss >> utf;
			// std::cout << utf << std::endl;

			std::cout << std::endl;
			if (auto acf = utf.find_col("CueNameTable"); acf != utf.end()) {
				if (auto data = acf->second.cast_at<UTF::field::data_t>(0)) {
					const auto [offset, size] = data.value();
					std::vector<char> buffer2(size, '\0');
					iss.seekg(offset, std::ios::beg).read(buffer2.data(), size);
					UTF utf2;
					std::ispanstream(buffer2, std::ios::binary) >> utf2;
					std::cout << utf2;
				}
			}

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
				if (l.empty())
					continue;
				std::cout << l << '\n';
			}
		} else if (extract_file.ends_with(".mbs")) {
			const MBS mbs = MBS::From(iss);
			std::cout << mbs.filename() << std::endl;
			// auto q = mbs.extract();
		}
	} else {
		std::cout << extract_file << " not found.\n";
		return 0;
	}

	return 0;
} catch (const std::exception& e) {
	std::cerr << e.what() << std::endl;
	return 1;
}