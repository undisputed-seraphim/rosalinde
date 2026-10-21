#include "dump.hpp"

#include <impl/dbreg.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
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

float as_float(uint32_t value) {
	float result = 0;
	std::memcpy(&result, &value, sizeof(result));
	return result;
}

} // namespace

int dump_dbreg(const context& ctx, std::span<char> data) {
	auto parsed = dbreg::parse(as_bytes(data));
	if (!parsed) {
		std::cerr << "d: " << ctx.path << ": not a valid DRG file\n";
		return 1;
	}
	const auto& file = *parsed;
	const bool catalog = file.type == dbreg::kind::catalog;
	const size_t key_limit = row_limit(ctx.opt, file.keys.size());
	const size_t value_limit = row_limit(ctx.opt, file.values.size());

	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, catalog ? "dbreg-catalog" : "dbreg-values");
		json.field("kind", catalog ? "catalog" : "values");
		if (catalog) {
			json.field("source_count", file.sources.size());
			json.field("key_count", file.keys.size());
			json.field("key_offset", file.hdr.key_offset);
			json.begin_array("sources");
			for (size_t i = 0; i < file.sources.size(); ++i) {
				const auto& source = file.sources[i];
				json.begin_object();
				json.field("index", i);
				json.field("path", source.path_view());
				json.field("tag", source.tag_view());
				json.begin_array("unknown");
				for (uint32_t value : source.unknown)
					json.number(value);
				json.end_array();
				json.end_object();
			}
			json.end_array();
			if (key_limit != file.keys.size())
				json.field("keys_truncated", true);
			json.begin_array("keys");
			for (size_t i = 0; i < key_limit; ++i) {
				const auto& key = file.keys[i];
				json.begin_object();
				json.field("index", i);
				json.field("key", key.key_view());
				json.field("flags", key.flags);
				json.field("default_value", key.default_value);
				json.field("unknown0", key.unknown0);
				json.field("unknown1", key.unknown1);
				json.field("description", key.description_view());
				json.end_object();
			}
			json.end_array();
		} else {
			json.field("value_count", file.values.size());
			if (value_limit != file.values.size())
				json.field("values_truncated", true);
			json.begin_array("values");
			for (size_t i = 0; i < value_limit; ++i) {
				json.begin_object();
				json.field("index", i);
				json.field("raw", file.values[i]);
				json.field("f32", as_float(file.values[i]));
				json.end_object();
			}
			json.end_array();
		}
		json.end_object();
		std::cout << '\n';
		return 0;
	}

	write_metadata_text(ctx, catalog ? "dbreg-catalog" : "dbreg-values");
	if (catalog) {
		std::cout << "source_count=" << file.sources.size() << " key_count=" << file.keys.size()
				  << " key_offset=" << file.hdr.key_offset << '\n';
		std::cout << "sources:\n";
		for (size_t i = 0; i < file.sources.size(); ++i) {
			const auto& source = file.sources[i];
			std::cout << "  [" << i << "] " << source.tag_view() << " | " << source.path_view() << '\n';
		}
		std::cout << "keys:\n";
		for (size_t i = 0; i < key_limit; ++i) {
			const auto& key = file.keys[i];
			std::cout << "  [" << i << "] " << key.key_view() << " default=" << key.default_value
					  << " flags=" << key.flags << '\n';
			if (!key.description_view().empty())
				std::cout << "      " << key.description_view() << '\n';
		}
		if (key_limit != file.keys.size())
			std::cout << "  ... " << (file.keys.size() - key_limit) << " more keys\n";
	} else {
		std::cout << "value_count=" << file.values.size() << '\n';
		std::cout << "values:\n";
		for (size_t i = 0; i < value_limit; ++i)
			std::cout << "  [" << i << "] raw=" << file.values[i] << " f32=" << as_float(file.values[i])
					  << '\n';
		if (value_limit != file.values.size())
			std::cout << "  ... " << (file.values.size() - value_limit) << " more values\n";
	}
	return 0;
}

} // namespace dump
