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

		for (size_t fi = 0; fi < file.functions.size(); ++fi) {
			const auto& fn = file.functions[fi];
			std::printf("\n  [%zu] %s (%zu bytes)\n", fi, fn.name.c_str(), fn.bytecode.size());

			size_t pc = 0;
			while (pc < fn.bytecode.size()) {
				uint8_t op = fn.bytecode[pc];
				std::printf("    %04zx: %02x %-12s", pc, op, asb::bc_opcode_name(op));
				pc++;
				if (pc < fn.bytecode.size()) {
					std::printf(" [");
					size_t start = pc;
					size_t extra = 0;
					switch (op) {
					case 0x00: case 0x01: case 0x04: case 0x05: case 0x06:
					case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
					case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d:
					case 0x2e: case 0x2f: case 0x39: case 0x3f: case 0x41:
					case 0x4a: case 0x4b: case 0x4c: case 0x50: case 0x52:
					case 0x98: case 0x99: case 0xb0:
						extra = 0; break;
					case 0x08: case 0x09: case 0x0b: case 0x0c: case 0x0d:
					case 0x0e: case 0x0f: case 0x10: case 0x11: case 0x12:
					case 0x13: case 0x3a: case 0x3c: case 0x3d: case 0x3e:
					case 0x40: case 0x42: case 0x44: case 0x60: case 0x61:
					case 0x64: case 0x65: case 0x68: case 0x6a: case 0x78:
						extra = (op == 0x65 || op == 0x60 || op == 0x61) ? 8 :
								(op == 0x3a || op == 0x43) ? 8 : 4; break;
					case 0x62: case 0x63: case 0x66: case 0x67: case 0x69:
						extra = (op == 0x63) ? 4 : 2; break;
					case 0x70: extra = 6; break;
					default: extra = 0; break;
					}
					for (size_t i = 0; i < extra && pc + i < fn.bytecode.size(); ++i) {
						if (i > 0) std::printf(" ");
						std::printf("%02x", fn.bytecode[pc + i]);
					}
					pc += extra;
					std::printf("]");
				}
				std::printf("\n");
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
