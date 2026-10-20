#include "dump.hpp"

#include <impl/bbs.hpp>

#include <algorithm>
#include <cstdint>
#include <format>
#include <iostream>
#include <iterator>
#include <span>
#include <string>

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

template <size_t N>
std::string hex_bytes(const std::array<uint8_t, N>& bytes) {
	std::string result;
	result.reserve(bytes.size() * 2);
	for (uint8_t byte : bytes)
		std::format_to(std::back_inserter(result), "{:02x}", byte);
	return result;
}

} // namespace

int dump_bbs(const context& ctx, std::span<char> data) {
	auto parsed = bbs::parse(as_bytes(data));
	if (!parsed) {
		std::cerr << "d: " << ctx.path << ": not a valid BBS file\n";
		return 1;
	}
	const auto& file = *parsed;

	const size_t color_limit = row_limit(ctx.opt, file.color_keys.size());
	const size_t section72_limit = row_limit(ctx.opt, file.section_72.size());
	const size_t section108_limit = row_limit(ctx.opt, file.section_108.size());
	const size_t section68_limit = row_limit(ctx.opt, file.section_68.size());
	const size_t section24_limit = row_limit(ctx.opt, file.section_24.size());
	const size_t section128_limit = row_limit(ctx.opt, file.section_128.size());
	const size_t section16_limit = row_limit(ctx.opt, file.section_16.size());

	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "bbs");
		json.field("version", file.hdr.version);
		json.field("mbs_count", file.mbs_names.size());
		json.field("ftx_count", file.ftx_names.size());
		json.field("color_key_count", file.color_keys.size());
		json.field("section_72_count", file.section_72.size());
		json.field("section_108_count", file.section_108.size());
		json.field("section_68_count", file.section_68.size());
		json.field("section_24_count", file.section_24.size());
		json.field("section_128_count", file.section_128.size());
		json.field("section_16_count", file.section_16.size());
		json.begin_array("mbs_names");
		for (const auto& name : file.mbs_names)
			json.string(name);
		json.end_array();
		json.begin_array("ftx_names");
		for (const auto& name : file.ftx_names)
			json.string(name);
		json.end_array();

		if (color_limit != file.color_keys.size())
			json.field("color_keys_truncated", true);
		json.begin_array("color_keys");
		for (size_t i = 0; i < color_limit; ++i) {
			const auto& key = file.color_keys[i];
			json.begin_object();
			json.field("index", i);
			json.field("r", key.r);
			json.field("g", key.g);
			json.field("b", key.b);
			json.field("flags", key.flags);
			json.end_object();
		}
		json.end_array();

		const auto write_raw = [&](const char* name, const auto& records, size_t limit) {
			if (limit != records.size())
				json.field(std::format("{}_truncated", name), true);
			json.begin_array(name);
			for (size_t i = 0; i < limit; ++i) {
				json.begin_object();
				json.field("index", i);
				json.field("hex", hex_bytes(records[i].data));
				json.end_object();
			}
			json.end_array();
		};
		write_raw("section_72", file.section_72, section72_limit);
		write_raw("section_108", file.section_108, section108_limit);
		write_raw("section_68", file.section_68, section68_limit);
		write_raw("section_24", file.section_24, section24_limit);
		write_raw("section_128", file.section_128, section128_limit);
		write_raw("section_16", file.section_16, section16_limit);
		json.end_object();
		std::cout << '\n';
		return 0;
	}

	write_metadata_text(ctx, "bbs");
	std::cout << "version=" << file.hdr.version << " mbs=" << file.mbs_names.size()
			  << " ftx=" << file.ftx_names.size() << " color_keys=" << file.color_keys.size()
			  << " s72=" << file.section_72.size() << " s108=" << file.section_108.size()
			  << " s68=" << file.section_68.size() << " s24=" << file.section_24.size()
			  << " s128=" << file.section_128.size() << " s16=" << file.section_16.size() << '\n';
	std::cout << "mbs_names:\n";
	for (size_t i = 0; i < file.mbs_names.size(); ++i)
		std::cout << "  [" << i << "] " << file.mbs_names[i] << '\n';
	std::cout << "ftx_names:\n";
	for (size_t i = 0; i < file.ftx_names.size(); ++i)
		std::cout << "  [" << i << "] " << file.ftx_names[i] << '\n';

	std::cout << "color_keys:\n";
	for (size_t i = 0; i < color_limit; ++i) {
		const auto& key = file.color_keys[i];
		std::cout << "  [" << i << "] r=" << key.r << " g=" << key.g << " b=" << key.b
				  << " flags=" << key.flags << '\n';
	}
	if (color_limit != file.color_keys.size())
		std::cout << "  ... " << (file.color_keys.size() - color_limit) << " more\n";

	const auto write_raw = [&](const char* name, const auto& records, size_t limit) {
		std::cout << name << ":\n";
		for (size_t i = 0; i < limit; ++i)
			std::cout << "  [" << i << "] " << hex_bytes(records[i].data) << '\n';
		if (limit != records.size())
			std::cout << "  ... " << (records.size() - limit) << " more\n";
	};
	write_raw("section_72", file.section_72, section72_limit);
	write_raw("section_108", file.section_108, section108_limit);
	write_raw("section_68", file.section_68, section68_limit);
	write_raw("section_24", file.section_24, section24_limit);
	write_raw("section_128", file.section_128, section128_limit);
	write_raw("section_16", file.section_16, section16_limit);
	return 0;
}

} // namespace dump
