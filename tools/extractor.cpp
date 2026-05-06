#include <boost/program_options.hpp>
#include <criware/cpk.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <string_view>

namespace fs = std::filesystem;
namespace po = boost::program_options;

// ============================================================================
// Data types
// ============================================================================

struct Entry {
	std::string dir;
	std::string name;
	uint64_t file_size;
	uint64_t extract_size;
	uint64_t offset;
	uint64_t id;

	double ratio() const noexcept {
		if (file_size == 0) return 0.0;
		return static_cast<double>(extract_size) / static_cast<double>(file_size);
	}
};

struct Args {
	std::string cpk_path;
	bool list = false;
	std::string extract_file;
	bool help = false;
};

// ============================================================================
// Core operations
// ============================================================================

CPKTable open_cpk(const std::string& path) {
	fs::path p = fs::absolute(fs::path(path));
	if (!fs::exists(p) || !fs::is_regular_file(p)) {
		throw std::runtime_error("File does not exist: " + p.string());
	}
	return TopLevelCpk(p).getTableOfContents();
}

std::vector<Entry> list_entries(const CPKTable& table) {
	std::vector<Entry> entries;
	entries.reserve(table.size());
	for (const auto& [DirName, FileName, FileSize, ExtractSize, FileOffset, ID] : table) {
		entries.emplace_back(
			DirName, FileName, FileSize, ExtractSize, FileOffset, ID);
	}
	return entries;
}

std::vector<char> extract_entry(
	const CPKTable& table, std::string_view dir, std::string_view name) {
	std::vector<char> buffer;
	table.extract(dir, name, buffer);
	if (buffer.empty()) {
		throw std::runtime_error("File not found or empty: "
			+ std::string(dir) + "/" + std::string(name));
	}
	return buffer;
}

std::pair<std::string_view, std::string_view>
split_path(std::string_view path) {
	auto pos = path.find_last_of('/');
	if (pos == std::string_view::npos) {
		return {"", path};
	}
	return {path.substr(0, pos), path.substr(pos + 1)};
}

void write_file(const fs::path& path, std::span<const char> data) {
	std::ofstream(path, std::ios::binary).write(data.data(), data.size());
}

// ============================================================================
// CLI output helpers
// ============================================================================

void print_list(const std::vector<Entry>& entries) {
	for (const auto& e : entries) {
		std::cout << e.id << ": " << e.dir << "/" << e.name
				  << "  compressed=" << e.file_size
				  << "  decompressed=" << e.extract_size
				  << "  ratio=" << std::fixed << std::setprecision(2)
				  << e.ratio() << "\n";
	}
	std::cout << entries.size() << " files\n";
}

// ============================================================================
// CLI
// ============================================================================

int main(int argc, char* argv[]) try {
	Args args;
	po::options_description desc;
	desc.add_options()("help,h", "Print this help message")(
		"cpk", po::value<std::string>(&args.cpk_path)->required(),
		"Path to CPK archive")(
		"list,l", po::bool_switch(&args.list), "List files with sizes")(
		"extract,e", po::value<std::string>(&args.extract_file),
		"Extract file (DirName/FileName)");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);

	try {
		po::notify(vm);
		if (vm.count("help")) {
			std::cout << desc << '\n';
			return 1;
		}
	} catch (const po::required_option& e) {
		std::cout << desc << '\n';
		throw;
	}

	auto table = open_cpk(args.cpk_path);

	if (args.list) {
		print_list(list_entries(table));
		return 0;
	}

	if (!args.extract_file.empty()) {
		auto [dir, name] = split_path(args.extract_file);
		auto data = extract_entry(table, dir, name);
		auto out_path = fs::path(name);
		write_file(out_path, data);
		std::cout << "Extracted " << dir << "/" << name
				  << " (" << data.size() << " bytes) -> " << out_path << "\n";
		return 0;
	}

	std::cout << desc << '\n';
	return 1;
} catch (const std::exception& e) {
	std::cerr << "Error: " << e.what() << '\n';
	return 1;
} catch (...) {
	std::cerr << "Unknown error.\n";
	return 1;
}
