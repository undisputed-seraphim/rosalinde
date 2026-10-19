#include "dump.hpp"

#include <impl/mpd.hpp>

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

int dump_mpd(const context& ctx, std::span<char> data) {
	auto parsed = mpd::parse(as_bytes(data));
	if (!parsed) {
		std::cerr << "d: " << ctx.path << ": not a valid MPD file\n";
		return 1;
	}
	const auto& file = *parsed;
	const size_t record_limit = row_limit(ctx.opt, file.records.size());

	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "mpd");
		json.field("version", file.hdr.version);
		json.field("header_size", file.hdr.header_size);
		json.field("data_size", file.hdr.data_size);
		json.field("record_count", file.records.size());
		if (record_limit != file.records.size())
			json.field("records_truncated", true);
		json.begin_array("records");
		for (size_t i = 0; i < record_limit; ++i) {
			const auto& record = file.records[i];
			json.begin_object();
			json.field("index", i);
			json.field("id", record.id);
			json.field("name", record.name_view());
			json.field("texture_index", record.texture_index());
			json.field("texture_flags", record.texture_flags);
			json.field("flags", record.flags());
			json.begin_array("vertices");
			for (const auto& vertex : record.vertices) {
				json.begin_object();
				json.field("x", vertex.x);
				json.field("y", vertex.y);
				json.field("z", vertex.z);
				json.field("color", vertex.color);
				json.field("u", vertex.u);
				json.field("v", vertex.v);
				json.end_object();
			}
			json.end_array();
			json.end_object();
		}
		json.end_array();
		json.end_object();
		std::cout << '\n';
		return 0;
	}

	write_metadata_text(ctx, "mpd");
	std::cout << "version=" << file.hdr.version << " records=" << file.records.size()
			  << " header_size=" << file.hdr.header_size << " data_size=" << file.hdr.data_size << '\n';
	std::cout << "records:\n";
	for (size_t i = 0; i < record_limit; ++i) {
		const auto& record = file.records[i];
		std::cout << "  [" << i << "] id=" << record.id << " name=\"" << record.name_view()
				  << "\" texture=" << record.texture_index() << " flags=" << record.flags() << '\n';
		for (size_t j = 0; j < 4; ++j) {
			const auto& vertex = record.vertices[j];
			std::cout << "    v" << j << " pos=(" << vertex.x << "," << vertex.y << "," << vertex.z
					  << ") color=" << vertex.color << " uv=(" << vertex.u << "," << vertex.v << ")\n";
		}
	}
	if (record_limit != file.records.size())
		std::cout << "  ... " << (file.records.size() - record_limit) << " more records\n";
	return 0;
}

} // namespace dump
