#include "dump.hpp"

#include <impl/ucb.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

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

struct group_stat {
	uint64_t count = 0;
	uint32_t min_x = std::numeric_limits<uint32_t>::max();
	uint32_t min_y = std::numeric_limits<uint32_t>::max();
	uint32_t max_x = 0;
	uint32_t max_y = 0;

	void add(const ucb::cell& cell) {
		++count;
		min_x = std::min(min_x, cell.x);
		min_y = std::min(min_y, cell.y);
		max_x = std::max(max_x, cell.x);
		max_y = std::max(max_y, cell.y);
	}
};

void write_group_json(json_writer& json, const char* name, const std::vector<group_stat>& groups) {
	json.begin_array(name);
	for (size_t i = 0; i < groups.size(); ++i) {
		const auto& group = groups[i];
		if (group.count == 0)
			continue;
		json.begin_object();
		json.field("index", i);
		json.field("count", group.count);
		json.field("min_x", group.min_x);
		json.field("min_y", group.min_y);
		json.field("max_x", group.max_x);
		json.field("max_y", group.max_y);
		json.end_object();
	}
	json.end_array();
}

int dump_ucb_stats(const context& ctx, const ucb::file& file) {
	std::vector<group_stat> objects(1024);
	std::vector<group_stat> entries(65536);
	std::map<uint32_t, uint64_t> flags;
	uint64_t occupied = 0;
	uint64_t empty = 0;
	uint64_t reserved_nonzero = 0;

	for (const auto& cell : file.cells) {
		const auto object = cell.object_index();
		const auto entry = cell.entry_index();
		if (cell.occupied()) {
			++occupied;
			if (object < objects.size())
				objects[object].add(cell);
			if (entry < entries.size())
				entries[entry].add(cell);
		} else {
			++empty;
		}
		if (cell.reserved_bits() != 0)
			++reserved_nonzero;
		++flags[cell.flags];
	}

	size_t object_count = 0;
	size_t entry_count = 0;
	for (const auto& group : objects)
		object_count += group.count != 0;
	for (const auto& group : entries)
		entry_count += group.count != 0;

	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "ucb");
		json.field("width", file.width);
		json.field("height", file.height);
		json.field("cell_size", ucb::cell_size);
		json.field("cell_count", file.cells.size());
		json.field("occupied", occupied);
		json.field("empty", empty);
		json.field("object_count", object_count);
		json.field("entry_count", entry_count);
		json.field("reserved_nonzero", reserved_nonzero);
		write_group_json(json, "objects", objects);
		write_group_json(json, "entries", entries);
		json.begin_array("flags");
		for (const auto& [value, count] : flags) {
			json.begin_object();
			json.field("value", value);
			json.field("count", count);
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
	std::cout << "occupied=" << occupied << " empty=" << empty << " objects=" << object_count
			  << " entries=" << entry_count << " reserved_nonzero=" << reserved_nonzero << '\n';
	return 0;
}

} // namespace

int dump_ucb(const context& ctx, std::span<char> data) {
	auto parsed = ucb::parse(as_bytes(data));
	if (!parsed) {
		std::cerr << "d: " << ctx.path << ": not a valid UCB file\n";
		return 1;
	}
	const auto& file = *parsed;
	if (ctx.opt.stats)
		return dump_ucb_stats(ctx, file);
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
