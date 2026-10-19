#include "dump.hpp"

#include <cctype>
#include <iostream>
#include <string>
#include <vector>

namespace dump {

namespace {

bool is_supported(std::string_view extension) {
	return extension == "acb" || extension == "acf" || extension == "nnd" || extension == "ftx" || extension == "mbs" ||
		   extension == "otm" || extension == "fms" || extension == "ufb" || extension == "ucb";
}

void report_unknown(const context& ctx, std::string_view extension) {
	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "unknown");
		json.field("extension", extension);
		json.field("status", "no inspector registered");
		json.end_object();
		std::cout << '\n';
		return;
	}

	write_metadata_text(ctx, "unknown");
	if (extension.empty())
		std::cout << "No inspector registered for a file without an extension.\n";
	else
		std::cout << "No inspector registered for ." << extension << ".\n";
}

void report_asb(const context& ctx) {
	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "asb");
		json.field("status", "deferred");
		json.end_object();
		std::cout << '\n';
		return;
	}

	write_metadata_text(ctx, "asb");
	std::cout << "ASB inspection is deferred.\n";
}

} // namespace

std::string lower_extension(std::string_view name) {
	const size_t dot = name.find_last_of('.');
	if (dot == std::string_view::npos || dot + 1 == name.size())
		return {};
	std::string extension(name.substr(dot + 1));
	for (char& c : extension)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return extension;
}

void write_metadata_text(const context& ctx, std::string_view type) {
	std::cout << "File: " << ctx.path << " (" << type << ", compressed=" << ctx.item.file_size
			  << ", decompressed=" << ctx.item.extract_size << ")\n";
}

void write_metadata_json(json_writer& json, const context& ctx, std::string_view type) {
	json.field("path", ctx.path);
	json.field("type", type);
	json.field("compressed_size", ctx.item.file_size);
	json.field("decompressed_size", ctx.item.extract_size);
}

void print_hex_preview(std::span<const char> data, size_t max_bytes) {
	const size_t size = data.size() < max_bytes ? data.size() : max_bytes;
	for (size_t i = 0; i < size; i += 16) {
		std::format_to(std::ostreambuf_iterator<char>(std::cout), "{:08x}  ", i);
		const size_t end = (i + 16 < size) ? i + 16 : size;
		for (size_t j = i; j < end; ++j) {
			const auto value = static_cast<unsigned char>(data[j]);
			std::format_to(std::ostreambuf_iterator<char>(std::cout), "{:02x} ", value);
		}
		for (size_t j = end; j < i + 16; ++j)
			std::cout << "   ";
		std::cout << " |";
		for (size_t j = i; j < end; ++j) {
			const auto value = static_cast<unsigned char>(data[j]);
			std::cout << (std::isprint(value) ? static_cast<char>(value) : '.');
		}
		std::cout << "|\n";
	}
}

int dispatch(const cpk::archive& archive, const cpk::entry& item, std::string_view path, const options& opt) {
	const std::string extension = lower_extension(item.name);
	const context ctx{path, item, opt};

	if (extension == "asb") {
		report_asb(ctx);
		return 1;
	}

	if (!is_supported(extension)) {
		report_unknown(ctx, extension);
		if (!opt.json && item.extract_size <= (1u << 20)) {
			try {
				std::vector<char> data;
				archive.extract(item, data);
				print_hex_preview(std::span<const char>(data.data(), data.size()));
			} catch (const std::exception& e) {
				std::cerr << "d: " << path << ": " << e.what() << '\n';
			}
		}
		return 1;
	}

	if (!opt.section.empty() && extension != "mbs") {
		std::cerr << "d: --section is only valid for mbs\n";
		return 2;
	}
	if ((opt.track || opt.sequence) && extension != "mbs") {
		std::cerr << "d: --track and --seq are only valid for mbs\n";
		return 2;
	}
	if (opt.sequence && !opt.track) {
		std::cerr << "d: --seq requires --track\n";
		return 2;
	}
	if ((opt.timeline || opt.colors) && (!opt.track || !opt.sequence)) {
		std::cerr << "d: --timeline/--colors require --track and --seq\n";
		return 2;
	}
	if (opt.track && (opt.full || !opt.section.empty())) {
		std::cerr << "d: --track/--seq cannot be combined with --full or --section\n";
		return 2;
	}

	std::vector<char> data;
	try {
		archive.extract(item, data);
	} catch (const std::exception& e) {
		std::cerr << "d: " << path << ": " << e.what() << '\n';
		return 1;
	}

	if (extension == "acb" || extension == "acf")
		return dump_acb(ctx, data);
	if (extension == "nnd")
		return dump_nnd(ctx, data);
	if (extension == "ftx")
		return dump_ftx(ctx, data);
	if (extension == "mbs")
		return dump_mbs(ctx, data);
	if (extension == "otm")
		return dump_otm(ctx, data);
	if (extension == "ufb")
		return dump_ufb(ctx, data);
	if (extension == "ucb")
		return dump_ucb(ctx, data);
	if (extension == "fms")
		return dump_fms(ctx, data);
	return 1;
}

} // namespace dump
