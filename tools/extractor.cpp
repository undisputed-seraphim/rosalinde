#include <boost/program_options.hpp>
#include <criware/byte_reader.hpp>
#include <criware/cpk.hpp>
#include <criware/utf.hpp>
#include <eltolinde.hpp>
#include <impl/asb.hpp>
#include <impl/mbs/csv_dump.hpp>
#include <impl/mbs/detail.hpp>
#include <impl/mbs/sections.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <span>
#include <string_view>
#include <vector>

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
	std::string dump_file;
	std::string mbs_dump;
	std::string mbs_out = "mbs_dump";
	std::string asb_dump;
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
	for (const auto& [DirName, FileName, FileSize, ExtractSize, FileOffset, ID] :
		 table.rows<"DirName", "FileName", "FileSize", "ExtractSize", "FileOffset", "ID">()) {
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

void dump_columns(const CPKTable& table, std::string_view file_path) {
	auto [dir, name] = split_path(file_path);
	auto data = extract_entry(table, dir, name);
	UTF::decipher(data);
	auto utf = UTF::parse(std::span(reinterpret_cast<const uint8_t*>(data.data()), data.size()));

	std::cout << "File: " << dir << "/" << name << "\n";
	std::cout << "Columns: " << utf.num_cols() << "\n";
	for (const auto& [col_name, field] : utf) {
		const char* type_str = "???";
		switch (field.type_) {
		case UTF::field::type::UINT8:  type_str = "u8"; break;
		case UTF::field::type::INT8:   type_str = "s8"; break;
		case UTF::field::type::UINT16: type_str = "u16"; break;
		case UTF::field::type::INT16:  type_str = "s16"; break;
		case UTF::field::type::UINT32: type_str = "u32"; break;
		case UTF::field::type::INT32:  type_str = "s32"; break;
		case UTF::field::type::UINT64: type_str = "u64"; break;
		case UTF::field::type::INT64:  type_str = "s64"; break;
		case UTF::field::type::FLOAT:  type_str = "f32"; break;
		case UTF::field::type::DOUBLE: type_str = "f64"; break;
		case UTF::field::type::STRING: type_str = "str"; break;
		case UTF::field::type::DATA:   type_str = "data"; break;
		default: break;
		}
		std::cout << "  " << col_name << " (" << type_str << ") x" << field.values.size();
		if (field.has_default) std::cout << " [default]";
		if (!field.valid) std::cout << " [invalid]";
		if (field.type_ == UTF::field::type::STRING && field.values.size() > 0) {
			auto s = field.cast_at<std::string>(0);
			if (s) std::cout << " = \"" << *s << "\"";
		}
		if (field.type_ == UTF::field::type::DATA && field.values.size() > 0) {
			auto d = field.cast_at<UTF::field::data_t>(0);
			if (d) std::cout << " offset=" << d->offset << " size=" << d->size;
		}
		std::cout << "\n";
	}
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
		"Extract file (DirName/FileName)")(
		"dump,d", po::value<std::string>(&args.dump_file),
		"Dump @UTF column layout of a file")(
		"mbs-dump", po::value<std::string>(&args.mbs_dump),
		"Dump MBS sections to CSV (DirName/FileName of .mbs in CPK)")(
		"mbs-out", po::value<std::string>(&args.mbs_out),
		"Output directory for MBS CSV dump (default: mbs_dump)")(
		"asb-dump", po::value<std::string>(&args.asb_dump),
		"Dump ASB compiled script (DirName/FileName of .asb in CPK)");

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

	if (!args.dump_file.empty()) {
		dump_columns(table, args.dump_file);
		return 0;
	}

	if (!args.mbs_dump.empty()) {
		auto [dir, name] = split_path(args.mbs_dump);
		auto data = extract_entry(table, dir, name);
		auto bytes = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(data.data()), data.size());

		byte_reader r(bytes);
		const auto h = r.read<mbs::file_header>();
		if (std::strncmp(h.magic, "FMBS", 4) != 0) {
			throw std::runtime_error("Not an FMBS file.");
		}

		mbs::v77 v;
		switch (h.version) {
		case 0x76:
			mbs::parse_v76(bytes, v);
			break;
		case 0x77:
			mbs::parse_v77(bytes, v);
			break;
		default:
			throw std::runtime_error("Unsupported FMBS version.");
		}

		fs::path outdir = fs::path(args.mbs_out) / name;
		mbs::dump_csv(v, outdir);
		std::cout << "Dumped " << dir << "/" << name
				  << " (v" << std::hex << h.version << std::dec
				  << ", " << v.s9.size() << " tracks) -> " << outdir << "/\n";
		return 0;
	}

	if (!args.asb_dump.empty()) {
		auto [dir, name] = split_path(args.asb_dump);
		auto data = extract_entry(table, dir, name);
		auto bytes = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(data.data()), data.size());

		auto file = asb::parse(bytes);

		std::printf("=== %s ===\n", file.source_path.c_str());
		std::printf("Header: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			file.header[0], file.header[1], file.header[2], file.header[3],
			file.header[4], file.header[5], file.header[6], file.header[7]);
		std::printf("Functions: %zu\n", file.functions.size());

		bool is_large = data.size() > 65536;
		constexpr size_t kMaxLines = 64;

		std::map<uint32_t, size_t> callsys_counts;

		for (size_t fi = 0; fi < file.functions.size(); ++fi) {
			const auto& fn = file.functions[fi];
			std::printf("\n  [%zu] %s (%zu bytes)\n", fi, fn.name.c_str(), fn.bytecode.size());

			if (is_large && fi > 0) {
				continue;
			}

			size_t pc = 0;
			size_t line = 0;
			while (pc < fn.bytecode.size() && line < kMaxLines) {
				uint8_t op = fn.bytecode[pc];
				int sz = asb::bc_instr_size(op);
				if (sz < 1) sz = 1;

				std::printf("    %04zx: %02x %-14s", pc, op, asb::bc_opcode_name(op));

				if (op == asb::BC_CALLSYS && pc + 5 < fn.bytecode.size()) {
					uint8_t pop = fn.bytecode[pc + 1];
					uint32_t fnid = fn.bytecode[pc + 2]
						| (fn.bytecode[pc + 3] << 8)
						| (fn.bytecode[pc + 4] << 16)
						| (fn.bytecode[pc + 5] << 24);
					std::printf(" #%u pop=%u", fnid, pop);
					callsys_counts[fnid]++;
				}
				std::printf("\n");

				if (pc + sz > fn.bytecode.size()) break;
				pc += sz;
				line++;
			}
			if (pc < fn.bytecode.size()) {
				std::printf("    ... (%zu more bytes)\n", fn.bytecode.size() - pc);
			}
		}

		if (!callsys_counts.empty()) {
			std::printf("\nCALLSYS targets:\n");
			for (auto [id, cnt] : callsys_counts) {
				std::printf("  #%u: %zu calls\n", id, cnt);
			}
		}
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
