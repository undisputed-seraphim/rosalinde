#include "dump.hpp"

#include <impl/fms.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>

namespace dump {
namespace {

std::span<const uint8_t> as_bytes(std::span<char> data) {
	return {reinterpret_cast<const uint8_t*>(data.data()), data.size()};
}

} // namespace

int dump_fms(const context& ctx, std::span<char> data) {
	auto parsed = fms::parse_fms(as_bytes(data));
	if (!parsed) {
		std::cerr << "d: " << ctx.path << ": not a valid FMS file\n";
		return 1;
	}
	const auto& table = *parsed;
	const size_t limit = ctx.opt.limit_set ? std::min(ctx.opt.limit, table.strings.size()) : table.strings.size();

	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "fms");
		json.field("data_size", table.data_size);
		json.field("header_size", table.header_size);
		json.field("footer_size", table.footer_size);
		json.field("string_count", table.strings.size());
		if (limit < table.strings.size())
			json.field("truncated", true);
		json.begin_array("strings");
		for (size_t i = 0; i < limit; ++i) {
			const auto& entry = table.strings[i];
			json.begin_object();
			json.field("index", i);
			json.field("prop1", entry.prop1);
			json.field("prop2", entry.prop2);
			json.field("text", entry.text);
			json.end_object();
		}
		json.end_array();
		json.end_object();
		std::cout << '\n';
		return 0;
	}

	write_metadata_text(ctx, "fms");
	std::cout << "data_size=" << table.data_size << " header_size=" << table.header_size
			  << " footer_size=" << table.footer_size << " strings=" << table.strings.size() << '\n';
	for (size_t i = 0; i < limit; ++i) {
		const auto& entry = table.strings[i];
		std::cout << "  [" << i << "] prop1=" << entry.prop1 << " prop2=" << entry.prop2 << " \"" << entry.text
				  << "\"\n";
	}
	if (limit < table.strings.size())
		std::cout << "  ... " << (table.strings.size() - limit) << " more strings\n";
	return 0;
}

} // namespace dump
