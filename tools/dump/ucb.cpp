#include "dump.hpp"

#include <impl/ucb.hpp>

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

int dump_ucb(const context& ctx, std::span<char> data) {
	auto parsed = ucb::parse(as_bytes(data));
	if (!parsed) {
		std::cerr << "d: " << ctx.path << ": not a valid UCB file\n";
		return 1;
	}
	const auto& file = *parsed;
	const size_t cell_limit = row_limit(ctx.opt, file.cells.size());

	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "ucb");
		json.field("width", file.width);
		json.field("height", file.height);
		json.field("cell_size", ucb::cell_size);
		json.field("cell_count", file.cells.size());
		if (cell_limit != file.cells.size())
			json.field("cells_truncated", true);
		json.begin_array("cells");
		for (size_t i = 0; i < cell_limit; ++i) {
			const auto& cell = file.cells[i];
			json.begin_object();
			json.field("index", i);
			json.field("x", cell.x);
			json.field("y", cell.y);
			json.field("value", cell.value);
			json.field("entry_index", cell.entry_index());
			json.field("occupied", cell.occupied());
			json.field("object_index", cell.object_index());
			json.field("reserved_bits", cell.reserved_bits());
			json.field("flags", cell.flags);
			json.end_object();
		}
		json.end_array();
		json.end_object();
		std::cout << '\n';
		return 0;
	}

	write_metadata_text(ctx, "ucb");
	std::cout << "grid=" << file.width << "x" << file.height << " cell_size=" << ucb::cell_size
			  << " cells=" << file.cells.size() << '\n';
	std::cout << "cells:\n";
	for (size_t i = 0; i < cell_limit; ++i) {
		const auto& cell = file.cells[i];
		std::cout << "  [" << i << "] x=" << cell.x << " y=" << cell.y << " value=" << cell.value
				  << " entry=" << cell.entry_index() << " occupied=" << cell.occupied()
				  << " object=" << cell.object_index() << " flags=" << cell.flags << '\n';
	}
	if (cell_limit != file.cells.size())
		std::cout << "  ... " << (file.cells.size() - cell_limit) << " more cells\n";
	return 0;
}

} // namespace dump
