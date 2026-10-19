#include "dump.hpp"

#include <impl/ufb.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>

namespace dump {
namespace {

std::span<const uint8_t> as_bytes(std::span<char> data) {
	return {reinterpret_cast<const uint8_t*>(data.data()), data.size()};
}

size_t row_limit(const options& opt, size_t rows) {
	if (opt.limit_set)
		return std::min(opt.limit, rows);
	if (opt.full)
		return rows;
	return std::min<size_t>(rows, 10);
}

} // namespace

int dump_ufb(const context& ctx, std::span<char> data) {
	auto parsed = ufb::parse(as_bytes(data));
	if (!parsed) {
		std::cerr << "d: " << ctx.path << ": not a valid UFB file\n";
		return 1;
	}
	const auto& file = *parsed;
	const size_t record_limit = row_limit(ctx.opt, file.records.size());
	const size_t string_limit = row_limit(ctx.opt, file.strings.size());

	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "ufb");
		json.field("version", file.hdr.version);
		json.field("header_size", file.hdr.header_size);
		json.field("flags", file.hdr.flags);
		json.field("cell_size", file.hdr.cell_size);
		json.field("width", file.hdr.width);
		json.field("height", file.hdr.height);
		json.field("min_x", file.hdr.min_x);
		json.field("min_y", file.hdr.min_y);
		json.field("max_x", file.hdr.max_x);
		json.field("max_y", file.hdr.max_y);
		json.field("size_x", file.hdr.size_x);
		json.field("size_y", file.hdr.size_y);
		json.field("cell_count", file.hdr.cell_count);
		json.field("count_44", file.hdr.count_44);
		json.field("count_48", file.hdr.count_48);
		json.field("count_4c", file.hdr.count_4c);
		json.field("count_50", file.hdr.count_50);
		json.field("count_54", file.hdr.count_54);
		json.field("count_58", file.hdr.count_58);
		json.field("count_5c", file.hdr.count_5c);
		json.field("asset_count", file.hdr.asset_count);
		json.field("object_name_count", file.hdr.object_name_count);
		json.field("tile_name_count", file.hdr.tile_name_count);
		json.field("region_name_count", file.hdr.region_name_count);
		json.field("record_count", file.records.size());
		json.field("string_count", file.strings.size());
		json.field("strings_offset", file.strings_offset);
		if (record_limit != file.records.size())
			json.field("records_truncated", true);
		json.begin_array("records");
		for (size_t i = 0; i < record_limit; ++i) {
			const auto& record = file.records[i];
			json.begin_object();
			json.field("index", i);
			json.field("head", record.head);
			json.field("f0", record.f0);
			json.field("f1", record.f1);
			json.field("f2", record.f2);
			json.field("f3", record.f3);
			json.field("f4", record.f4);
			json.end_object();
		}
		json.end_array();
		if (string_limit != file.assets.size())
			json.field("assets_truncated", true);
		json.begin_array("assets");
		for (size_t i = 0; i < string_limit && i < file.assets.size(); ++i) {
			json.begin_object();
			json.field("index", i);
			json.field("main", file.assets[i].main);
			json.field("texture", file.assets[i].texture);
			json.end_object();
		}
		json.end_array();
		if (string_limit != file.object_names.size())
			json.field("object_names_truncated", true);
		json.begin_array("object_names");
		for (size_t i = 0; i < string_limit && i < file.object_names.size(); ++i)
			json.string(file.object_names[i]);
		json.end_array();
		if (string_limit != file.tile_names.size())
			json.field("tile_names_truncated", true);
		json.begin_array("tile_names");
		for (size_t i = 0; i < string_limit && i < file.tile_names.size(); ++i)
			json.string(file.tile_names[i]);
		json.end_array();
		json.begin_array("region_names");
		for (const auto& name : file.region_names)
			json.string(name);
		json.end_array();
		json.end_object();
		std::cout << '\n';
		return 0;
	}

	write_metadata_text(ctx, "ufb");
	std::cout << "version=" << file.hdr.version << " grid=" << file.hdr.width << "x" << file.hdr.height
			  << " cell_size=" << file.hdr.cell_size << " cells=" << file.hdr.cell_count << '\n';
	std::cout << "bounds=(" << file.hdr.min_x << "," << file.hdr.min_y << ")-(" << file.hdr.max_x << ","
			  << file.hdr.max_y << ") size=(" << file.hdr.size_x << "," << file.hdr.size_y << ")\n";
	std::cout << "counts=" << file.hdr.count_44 << "," << file.hdr.count_48 << "," << file.hdr.count_4c << ","
			  << file.hdr.count_50 << "," << file.hdr.count_54 << "," << file.hdr.count_58 << ","
			  << file.hdr.count_5c << '\n';
	std::cout << "assets=" << file.assets.size() << " object_names=" << file.object_names.size()
			  << " tile_names=" << file.tile_names.size() << " region_names=" << file.region_names.size()
			  << " records=" << file.records.size() << " strings=" << file.strings.size()
			  << " strings_offset=" << file.strings_offset << '\n';

	std::cout << "assets:\n";
	for (size_t i = 0; i < string_limit && i < file.assets.size(); ++i)
		std::cout << "  [" << i << "] " << file.assets[i].main << " | " << file.assets[i].texture << '\n';
	if (string_limit < file.assets.size())
		std::cout << "  ... " << (file.assets.size() - string_limit) << " more assets\n";

	std::cout << "object_names:\n";
	for (size_t i = 0; i < string_limit && i < file.object_names.size(); ++i)
		std::cout << "  [" << i << "] " << file.object_names[i] << '\n';
	if (string_limit < file.object_names.size())
		std::cout << "  ... " << (file.object_names.size() - string_limit) << " more object names\n";

	std::cout << "tile_names:\n";
	for (size_t i = 0; i < string_limit && i < file.tile_names.size(); ++i)
		std::cout << "  [" << i << "] " << file.tile_names[i] << '\n';
	if (string_limit < file.tile_names.size())
		std::cout << "  ... " << (file.tile_names.size() - string_limit) << " more tile names\n";

	std::cout << "region_names:\n";
	for (const auto& name : file.region_names)
		std::cout << "  " << name << '\n';

	std::cout << "records:\n";
	for (size_t i = 0; i < record_limit; ++i) {
		const auto& record = file.records[i];
		std::cout << "  [" << i << "] head=" << record.head << " f=(" << record.f0 << "," << record.f1 << ","
				  << record.f2 << "," << record.f3 << "," << record.f4 << ")\n";
	}
	if (record_limit != file.records.size())
		std::cout << "  ... " << (file.records.size() - record_limit) << " more records\n";
	return 0;
}

} // namespace dump
