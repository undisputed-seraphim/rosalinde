#include "dump.hpp"

#include <impl/ftx.hpp>

#include <iostream>

namespace dump {
namespace {

const char* compression_name(FTX::Compression compression) {
	switch (compression) {
	case FTX::Compression::NONE:
		return "NONE";
	case FTX::Compression::BC3_UNORM:
		return "BC3_UNORM";
	case FTX::Compression::BC3_SRGB:
		return "BC3_SRGB";
	case FTX::Compression::BC4_UNORM:
		return "BC4_UNORM";
	case FTX::Compression::BC4_SNORM:
		return "BC4_SNORM";
	case FTX::Compression::BC7_UNORM:
		return "BC7_UNORM";
	case FTX::Compression::BC7_SRGB:
		return "BC7_SRGB";
	}
	return "UNKNOWN";
}

} // namespace

int dump_ftx(const context& ctx, std::span<char> data) {
	const auto entries = FTX::parse(data);
	if (entries.empty()) {
		std::cerr << "d: " << ctx.path << ": not a valid FTX texture\n";
		return 1;
	}

	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "ftx");
		json.begin_array("entries");
		for (const auto& entry : entries) {
			json.begin_object();
			json.field("name", entry.name);
			json.field("width", entry.width);
			json.field("height", entry.height);
			json.field("compression", compression_name(entry.comp));
			json.field("compressed_size", entry.compressed_size);
			json.field("uncompressed_size", entry.uncompressed_size);
			json.end_object();
		}
		json.end_array();
		json.end_object();
		std::cout << '\n';
		return 0;
	}

	write_metadata_text(ctx, "ftx");
	std::cout << "entries=" << entries.size() << '\n';
	for (size_t i = 0; i < entries.size(); ++i) {
		const auto& entry = entries[i];
		std::cout << "[" << i << "] " << entry.name << " " << entry.width << "x" << entry.height << " "
				  << compression_name(entry.comp) << " compressed=" << entry.compressed_size
				  << " uncompressed=" << entry.uncompressed_size << '\n';
	}
	return 0;
}

} // namespace dump
