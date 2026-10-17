#include <boost/program_options.hpp>
#include <criware/byte_reader.hpp>
#include <criware/cpk.hpp>
#include <criware/utf.hpp>
#include <eltolinde.hpp>
#include <impl/mbs/csv_dump.hpp>
#include <impl/mbs/detail.hpp>
#include <impl/mbs/sections.hpp>
#include <impl/nnd.hpp>
#include <impl/nnd_dump.hpp>
#include <impl/otm.hpp>
#include <impl/otm_dump.hpp>

#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
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
		if (file_size == 0)
			return 0.0;
		return static_cast<double>(extract_size) / static_cast<double>(file_size);
	}
};

struct Args {
	std::string cpk_path;
	bool list = false;
	std::string extract_file;
	std::string dump_file;
	std::string mbs_dump;
	std::string mbs_info;
	int mbs_track = -1;
	int mbs_run = -1;
	std::string mbs_out = "mbs_dump";
	std::string otm_dump;
	std::string otm_out = "otm_dump";
	bool otm_check = false;
	std::string nnd_dump;
	std::string nnd_out = "nnd_dump";
	bool nnd_check = false;
	std::string extract_all;
	std::string skip_ext;
	uint64_t max_size_mb = 0;
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
		entries.emplace_back(DirName, FileName, FileSize, ExtractSize, FileOffset, ID);
	}
	return entries;
}

std::vector<char> extract_entry(const CPKTable& table, std::string_view dir, std::string_view name) {
	std::vector<char> buffer;
	table.extract(dir, name, buffer);
	if (buffer.empty()) {
		throw std::runtime_error("File not found or empty: " + std::string(dir) + "/" + std::string(name));
	}
	return buffer;
}

std::pair<std::string_view, std::string_view> split_path(std::string_view path) {
	auto pos = path.find_last_of('/');
	if (pos == std::string_view::npos) {
		return {"", path};
	}
	return {path.substr(0, pos), path.substr(pos + 1)};
}

void write_file(const fs::path& path, std::span<const char> data) {
	std::ofstream(path, std::ios::binary).write(data.data(), data.size());
}

std::string json_quote(std::string_view value) {
	std::string out;
	out.reserve(value.size() + 8);
	for (unsigned char c : value) {
		switch (c) {
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if (c < 0x20) {
				char buf[8];
				std::snprintf(buf, sizeof(buf), "\\u%04x", c);
				out += buf;
			} else {
				out += static_cast<char>(c);
			}
		}
	}
	return out;
}

std::string json_number(double value) {
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%.7g", value);
	return buf;
}

std::string json_bool(bool value) { return value ? "true" : "false"; }

std::string mbs_name(const char (&name)[24]) {
	size_t n = 0;
	while (n < 24 && name[n] != '\0')
		++n;
	return std::string(name, n);
}

void dump_mbs_info(const mbs::v77& v, uint16_t version, std::string_view file, int only_track, int only_run) {
	if (only_track >= static_cast<int>(v.s9.size()))
		throw std::runtime_error("track index out of range");
	if (only_run >= 0 && only_track < 0)
		throw std::runtime_error("--mbs-run requires --mbs-track");

	std::cout << "{\n";
	std::cout << "  \"file\": \"" << json_quote(file) << "\",\n";
	std::cout << "  \"version\": " << version << ",\n";
	std::cout << "  \"track_count\": " << v.s9.size() << ",\n";
	std::cout << "  \"tracks\": [";

	bool first_track = true;
	const int begin = only_track >= 0 ? only_track : 0;
	const int end = only_track >= 0 ? only_track + 1 : static_cast<int>(v.s9.size());
	for (int ti = begin; ti < end; ++ti) {
		const auto& s9 = v.s9[ti];
		std::cout << (first_track ? "\n" : ",\n");
		first_track = false;
		std::cout << "    {\n";
		std::cout << "      \"index\": " << ti << ",\n";
		std::cout << "      \"name\": \"" << json_quote(mbs_name(s9.name)) << "\",\n";
		std::cout << "      \"bounds\": [" << json_number(s9.left) << ", " << json_number(s9.top) << ", "
				  << json_number(s9.right) << ", " << json_number(s9.bottom) << "],\n";
		std::cout << "      \"sa_set_id\": " << s9.sa_set_id << ",\n";
		std::cout << "      \"sa_set_no\": " << static_cast<unsigned>(s9.sa_set_no) << ",\n";
		std::cout << "      \"sa_set_main\": " << static_cast<unsigned>(s9.sa_set_main) << ",\n";
		std::cout << "      \"sa_sb_set_id\": " << s9.sa_sb_set_id << ",\n";
		std::cout << "      \"sa_sb_set_no\": " << static_cast<unsigned>(s9.sa_sb_set_no) << ",\n";
		std::cout << "      \"disabled\": " << json_bool(s9.disabled != 0) << ",\n";
		std::cout << "      \"runs\": [";

		const int run_begin = only_run >= 0 && ti == only_track ? only_run : 0;
		const int run_end = only_run >= 0 && ti == only_track ? only_run + 1 : static_cast<int>(s9.sa_set_no);
		if (run_end > static_cast<int>(s9.sa_set_no))
			throw std::runtime_error("run index out of range");
		bool first_run = true;
		for (int ri = run_begin; ri < run_end; ++ri) {
			const auto& sa = v.sa[s9.sa_set_id + ri];
			long total_frames = 0;
			uint32_t loop_start = 0;
			uint32_t acc = 0;
			for (uint32_t j = 0; j < sa.s8_no && sa.s8_id + j < v.s8.size(); ++j) {
				total_frames += v.s8[sa.s8_id + j].frames;
				acc += v.s8[sa.s8_id + j].frames;
				if (sa.s8_sum_once > 0 && loop_start == 0 && acc >= static_cast<uint32_t>(sa.s8_sum_once))
					loop_start = j + 1;
			}
			std::cout << (first_run ? "\n" : ",\n");
			first_run = false;
			std::cout << "        {\n";
			std::cout << "          \"index\": " << ri << ",\n";
			std::cout << "          \"s8_start\": " << sa.s8_id << ",\n";
			std::cout << "          \"s8_count\": " << sa.s8_no << ",\n";
			std::cout << "          \"s8_start_offset\": " << static_cast<unsigned>(sa.s8_st) << ",\n";
			std::cout << "          \"s8_sum\": " << sa.s8_sum << ",\n";
			std::cout << "          \"s8_sum_once\": " << sa.s8_sum_once << ",\n";
			std::cout << "          \"total_frames\": " << total_frames << ",\n";
			std::cout << "          \"loop_start\": " << loop_start << ",\n";
			std::cout << "          \"single_pass\": " << json_bool(sa.s8_sum_once < 0) << ",\n";
			std::cout << "          \"track_id\": " << sa.track_id;
			if (only_run >= 0 && ti == only_track) {
				std::cout << ",\n          \"s8\": [";
				bool first_s8 = true;
				for (uint32_t j = 0; j < sa.s8_no && sa.s8_id + j < v.s8.size(); ++j) {
					const auto& s8 = v.s8[sa.s8_id + j];
					std::cout << (first_s8 ? "\n" : ",\n");
					first_s8 = false;
					std::cout << "            {\"index\": " << j << ", \"s8_id\": " << (sa.s8_id + j)
							  << ", \"s7_id\": " << s8.s7_id << ", \"s6_id\": " << s8.s6_id
							  << ", \"frames\": " << s8.frames << ", \"flags\": " << s8.flags
							  << ", \"loop_s8_id\": " << s8.loop_s8_id << "}";
				}
				std::cout << "\n          ]";
			}
			std::cout << "\n        }";
		}
		std::cout << "\n      ]\n    }";
	}
	std::cout << "\n  ]\n}\n";
}

// ============================================================================
// CLI output helpers
// ============================================================================

void print_list(const std::vector<Entry>& entries) {
	for (const auto& e : entries) {
		std::cout << e.id << ": " << e.dir << "/" << e.name << "  compressed=" << e.file_size
				  << "  decompressed=" << e.extract_size << "  ratio=" << std::fixed << std::setprecision(2)
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
		case UTF::field::type::UINT8:
			type_str = "u8";
			break;
		case UTF::field::type::INT8:
			type_str = "s8";
			break;
		case UTF::field::type::UINT16:
			type_str = "u16";
			break;
		case UTF::field::type::INT16:
			type_str = "s16";
			break;
		case UTF::field::type::UINT32:
			type_str = "u32";
			break;
		case UTF::field::type::INT32:
			type_str = "s32";
			break;
		case UTF::field::type::UINT64:
			type_str = "u64";
			break;
		case UTF::field::type::INT64:
			type_str = "s64";
			break;
		case UTF::field::type::FLOAT:
			type_str = "f32";
			break;
		case UTF::field::type::DOUBLE:
			type_str = "f64";
			break;
		case UTF::field::type::STRING:
			type_str = "str";
			break;
		case UTF::field::type::DATA:
			type_str = "data";
			break;
		default:
			break;
		}
		std::cout << "  " << col_name << " (" << type_str << ") x" << field.values.size();
		if (field.has_default)
			std::cout << " [default]";
		if (!field.valid)
			std::cout << " [invalid]";
		if (field.type_ == UTF::field::type::STRING && field.values.size() > 0) {
			auto s = field.cast_at<std::string>(0);
			if (s)
				std::cout << " = \"" << *s << "\"";
		}
		if (field.type_ == UTF::field::type::DATA && field.values.size() > 0) {
			auto d = field.cast_at<UTF::field::data_t>(0);
			if (d)
				std::cout << " offset=" << d->offset << " size=" << d->size;
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
		"cpk", po::value<std::string>(&args.cpk_path)->required(), "Path to CPK archive")(
		"list,l", po::bool_switch(&args.list), "List files with sizes")(
		"extract,e", po::value<std::string>(&args.extract_file), "Extract file (DirName/FileName)")(
		"dump,d", po::value<std::string>(&args.dump_file), "Dump @UTF column layout of a file")(
		"mbs-dump",
		po::value<std::string>(&args.mbs_dump),
		"Dump MBS sections to CSV (DirName/FileName of .mbs in CPK)")(
		"mbs-info",
		po::value<std::string>(&args.mbs_info),
		"Dump interpreted MBS track/run metadata as JSON (DirName/FileName of .mbs in CPK)")(
		"mbs-track", po::value<int>(&args.mbs_track), "Limit --mbs-info to one track index")(
		"mbs-run", po::value<int>(&args.mbs_run), "Limit --mbs-info to one run index (requires --mbs-track)")(
		"mbs-out", po::value<std::string>(&args.mbs_out), "Output directory for MBS CSV dump (default: mbs_dump)")(
		"otm-dump",
		po::value<std::string>(&args.otm_dump),
		"Dump OTM header/tracks/events/cues to CSV (DirName/FileName of .otm in CPK)")(
		"otm-out",
		po::value<std::string>(&args.otm_out),
		"Output directory for OTM CSV dump (default: otm_dump)")(
		"otm-check", po::bool_switch(&args.otm_check), "Parse every OTM in the CPK and report failures")(
		"nnd-dump",
		po::value<std::string>(&args.nnd_dump),
		"Dump NND header/nodes/edges/names/links to CSV (DirName/FileName of .nnd in CPK)")(
		"nnd-out",
		po::value<std::string>(&args.nnd_out),
		"Output directory for NND CSV dump (default: nnd_dump)")(
		"nnd-check", po::bool_switch(&args.nnd_check), "Parse every NND in the CPK and report failures")(
		"extract-all",
		po::value<std::string>(&args.extract_all),
		"Extract every entry into DIR (preserving DirName/FileName)")(
		"skip-ext",
		po::value<std::string>(&args.skip_ext),
		"Extensions to skip when extracting all (e.g. ftx,acb,awb)")(
		"max-size-mb",
		po::value<uint64_t>(&args.max_size_mb)->default_value(0),
		"Skip entries whose decompressed size exceeds N MiB (0 = no limit)");

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

	if (!args.extract_all.empty()) {
		auto lower = [](std::string_view s) {
			std::string out(s);
			for (char& c : out)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return out;
		};

		std::set<std::string> skip;
		{
			std::string_view rest = args.skip_ext;
			while (!rest.empty()) {
				auto pos = rest.find(',');
				auto tok = rest.substr(0, pos);
				if (!tok.empty())
					skip.insert(lower(tok));
				if (pos == std::string_view::npos)
					break;
				rest = rest.substr(pos + 1);
			}
		}

		const uint64_t max_bytes = args.max_size_mb * 1024ull * 1024ull;
		size_t done = 0, skipped = 0;
		for (const auto& e : list_entries(table)) {
			auto dot = e.name.find_last_of('.');
			std::string ext = dot == std::string::npos ? std::string() : lower(e.name.substr(dot + 1));
			if (skip.count(ext)) {
				++skipped;
				continue;
			}
			if (max_bytes != 0 && e.extract_size > max_bytes) {
				++skipped;
				continue;
			}

			fs::path out_path = fs::path(args.extract_all) / e.dir / e.name;
			fs::create_directories(out_path.parent_path());

			std::vector<char> data;
			table.extract(e.dir, e.name, data);
			write_file(out_path, data);
			++done;
		}
		std::cout << "Extracted " << done << " files (" << skipped << " skipped) -> " << args.extract_all << "\n";
		return 0;
	}

	if (!args.extract_file.empty()) {
		auto [dir, name] = split_path(args.extract_file);
		auto data = extract_entry(table, dir, name);
		auto out_path = fs::path(name);
		write_file(out_path, data);
		std::cout << "Extracted " << dir << "/" << name << " (" << data.size() << " bytes) -> " << out_path << "\n";
		return 0;
	}

	if (!args.dump_file.empty()) {
		dump_columns(table, args.dump_file);
		return 0;
	}

	if (!args.mbs_info.empty()) {
		auto [dir, name] = split_path(args.mbs_info);
		auto data = extract_entry(table, dir, name);
		auto bytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size());

		byte_reader r(bytes);
		const auto h = r.read<mbs::file_header>();
		if (std::strncmp(h.magic, "FMBS", 4) != 0)
			throw std::runtime_error("Not an FMBS file.");

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
		dump_mbs_info(v, h.version, args.mbs_info, args.mbs_track, args.mbs_run);
		return 0;
	}

	if (!args.mbs_dump.empty()) {
		auto [dir, name] = split_path(args.mbs_dump);
		auto data = extract_entry(table, dir, name);
		auto bytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size());

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
		std::cout << "Dumped " << dir << "/" << name << " (v" << std::hex << h.version << std::dec << ", "
				  << v.s9.size() << " tracks) -> " << outdir << "/\n";
		return 0;
	}

	if (!args.otm_dump.empty()) {
		auto [dir, name] = split_path(args.otm_dump);
		auto data = extract_entry(table, dir, name);
		auto bytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size());

		auto f = otm::parse(bytes);
		if (!f)
			throw std::runtime_error("Not a valid OTBM file.");

		fs::path outdir = fs::path(args.otm_out) / name;
		otm::dump_csv(*f, outdir);
		std::cout << "Dumped " << dir << "/" << name << " (" << f->mbs_name << ", v" << f->hdr.version << ", "
				  << f->tracks.size() << " tracks, " << f->events.size() << " events, " << f->cues.size()
				  << " cues) -> " << outdir << "/\n";
		return 0;
	}

	if (args.otm_check) {
		size_t ok = 0;
		size_t failed = 0;
		for (const auto& e : list_entries(table)) {
			if (e.name.size() < 4)
				continue;
			auto ext = e.name.substr(e.name.size() - 4);
			for (char& c : ext)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			if (ext != ".otm")
				continue;

			std::vector<char> data;
			table.extract(e.dir, e.name, data);
			auto bytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size());
			if (otm::parse(bytes)) {
				++ok;
			} else {
				++failed;
				std::cout << "FAIL " << e.dir << "/" << e.name << "\n";
			}
		}
		std::cout << "Parsed " << ok << " OTM files (" << failed << " failed)\n";
		return failed == 0 ? 0 : 1;
	}

	if (!args.nnd_dump.empty()) {
		auto [dir, name] = split_path(args.nnd_dump);
		auto data = extract_entry(table, dir, name);
		auto bytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size());

		auto f = nnd::parse(bytes);
		if (!f)
			throw std::runtime_error("Not a valid NND file.");

		size_t edges = 0;
		size_t links = 0;
		for (const auto& n : f->nodes)
			edges += n.edges.size();
		for (const auto& n : f->names)
			links += n.links.size();

		fs::path outdir = fs::path(args.nnd_out) / name;
		nnd::dump_csv(*f, outdir);
		std::cout << "Dumped " << dir << "/" << name << " (v" << f->version << ", " << f->nodes.size()
				  << " nodes, " << edges << " edges, " << f->names.size() << " names, " << links
				  << " links) -> " << outdir << "/\n";
		return 0;
	}

	if (args.nnd_check) {
		size_t ok = 0;
		size_t failed = 0;
		for (const auto& e : list_entries(table)) {
			if (e.name.size() < 4)
				continue;
			auto ext = e.name.substr(e.name.size() - 4);
			for (char& c : ext)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			if (ext != ".nnd")
				continue;

			std::vector<char> data;
			table.extract(e.dir, e.name, data);
			auto bytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size());
			if (nnd::parse(bytes)) {
				++ok;
			} else {
				++failed;
				std::cout << "FAIL " << e.dir << "/" << e.name << "\n";
			}
		}
		std::cout << "Parsed " << ok << " NND files (" << failed << " failed)\n";
		return failed == 0 ? 0 : 1;
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
