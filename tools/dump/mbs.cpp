#include "dump.hpp"

#include <impl/mbs/detail.hpp>
#include <impl/mbs/sections.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace dump {
namespace {

constexpr std::array<std::string_view, 12>
	section_ids{"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "sa", "sb"};
constexpr std::array<std::string_view, 12> section_labels{
	"fog colors",
	"texture UVs",
	"vertices",
	"hitboxes",
	"keyframe layers",
	"hitbox entries",
	"keyframes",
	"transforms",
	"animation frames",
	"tracks",
	"sequences",
	"extras"};

struct cell {
	enum class kind { unsigned_number, signed_number, real, text, hex } type = kind::unsigned_number;
	uint64_t u = 0;
	int64_t i = 0;
	double d = 0;
	std::string_view text;

	static cell unum(uint64_t value) { return {kind::unsigned_number, value}; }
	static cell snum(int64_t value) { return {kind::signed_number, static_cast<uint64_t>(value), value}; }
	static cell real(double value) { return {kind::real, 0, 0, value}; }
	static cell str(std::string_view value) { return {kind::text, 0, 0, 0, value}; }
	static cell hex(uint64_t value) { return {kind::hex, value}; }
};

struct table {
	std::string_view id;
	std::string_view label;
	std::vector<std::string_view> columns;
	std::vector<std::vector<cell>> rows;
};

std::string_view fixed_name(const char (&name)[24]) {
	size_t size = 0;
	while (size < 24 && name[size] != '\0')
		++size;
	return {name, size};
}

void write_cell_text(const cell& value) {
	switch (value.type) {
	case cell::kind::unsigned_number:
		std::cout << value.u;
		break;
	case cell::kind::signed_number:
		std::cout << value.i;
		break;
	case cell::kind::real:
		std::cout << value.d;
		break;
	case cell::kind::text:
		std::cout << '"' << value.text << '"';
		break;
	case cell::kind::hex:
		std::cout << std::format("{:#x}", value.u);
		break;
	}
}

void write_cell_json(json_writer& json, const cell& value) {
	switch (value.type) {
	case cell::kind::unsigned_number:
	case cell::kind::hex:
		json.number(value.u);
		break;
	case cell::kind::signed_number:
		json.number(value.i);
		break;
	case cell::kind::real:
		json.number(value.d);
		break;
	case cell::kind::text:
		json.string(value.text);
		break;
	}
}

std::vector<table> build_tables(const mbs::v77& value) {
	std::vector<table> tables;
	tables.reserve(12);

	{
		table out{"s0", section_labels[0], {"center", "r", "g", "b", "a", "topleft"}, {}};
		out.rows.reserve(value.s0.size());
		for (const auto& item : value.s0) {
			out.rows.push_back(
				{cell::hex(item.center),
				 cell::unum(static_cast<uint8_t>(item.colors[0] >> 24)),
				 cell::unum(static_cast<uint8_t>(item.colors[0] >> 16)),
				 cell::unum(static_cast<uint8_t>(item.colors[0] >> 8)),
				 cell::unum(static_cast<uint8_t>(item.colors[0])),
				 cell::hex(item.topleft)});
		}
		tables.push_back(std::move(out));
	}

	{
		table out{
			"s1",
			section_labels[1],
			{"center_x",
			 "center_y",
			 "uv0_x",
			 "uv0_y",
			 "uv1_x",
			 "uv1_y",
			 "uv2_x",
			 "uv2_y",
			 "uv3_x",
			 "uv3_y",
			 "topleft_x",
			 "topleft_y"},
			{}};
		out.rows.reserve(value.s1.size());
		for (const auto& item : value.s1) {
			out.rows.push_back(
				{cell::real(item.center[0]),
				 cell::real(item.center[1]),
				 cell::real(item.values[0].x),
				 cell::real(item.values[0].y),
				 cell::real(item.values[1].x),
				 cell::real(item.values[1].y),
				 cell::real(item.values[2].x),
				 cell::real(item.values[2].y),
				 cell::real(item.values[3].x),
				 cell::real(item.values[3].y),
				 cell::real(item.topleft[0]),
				 cell::real(item.topleft[1])});
		}
		tables.push_back(std::move(out));
	}

	{
		table out{
			"s2",
			section_labels[2],
			{"unused_x",
			 "unused_y",
			 "v0_x",
			 "v0_y",
			 "v1_x",
			 "v1_y",
			 "v2_x",
			 "v2_y",
			 "v3_x",
			 "v3_y",
			 "topleft_x",
			 "topleft_y"},
			{}};
		out.rows.reserve(value.s2.size());
		for (const auto& item : value.s2) {
			out.rows.push_back(
				{cell::real(item.unused0[0]),
				 cell::real(item.unused0[1]),
				 cell::real(item.values[0].x),
				 cell::real(item.values[0].y),
				 cell::real(item.values[1].x),
				 cell::real(item.values[1].y),
				 cell::real(item.values[2].x),
				 cell::real(item.values[2].y),
				 cell::real(item.values[3].x),
				 cell::real(item.values[3].y),
				 cell::real(item.topleft[0]),
				 cell::real(item.topleft[1])});
		}
		tables.push_back(std::move(out));
	}

	{
		table out{
			"s3",
			section_labels[3],
			{"hb0_x", "hb0_y", "hb1_x", "hb1_y", "hb2_x", "hb2_y", "hb3_x", "hb3_y", "n0_x", "n0_y",
			 "n0_z",  "n1_x",  "n1_y",	"n1_z",	 "n2_x",  "n2_y",  "n2_z",	"n3_x",	 "n3_y", "n3_z"},
			{}};
		out.rows.reserve(value.s3.size());
		for (const auto& item : value.s3) {
			out.rows.push_back(
				{cell::real(item.hitbox[0].x),	cell::real(item.hitbox[0].y),  cell::real(item.hitbox[1].x),
				 cell::real(item.hitbox[1].y),	cell::real(item.hitbox[2].x),  cell::real(item.hitbox[2].y),
				 cell::real(item.hitbox[3].x),	cell::real(item.hitbox[3].y),  cell::real(item.normals[0].x),
				 cell::real(item.normals[0].y), cell::real(item.normals[0].z), cell::real(item.normals[1].x),
				 cell::real(item.normals[1].y), cell::real(item.normals[1].z), cell::real(item.normals[2].x),
				 cell::real(item.normals[2].y), cell::real(item.normals[2].z), cell::real(item.normals[3].x),
				 cell::real(item.normals[3].y), cell::real(item.normals[3].z)});
		}
		tables.push_back(std::move(out));
	}

	{
		table out{
			"s4",
			section_labels[4],
			{"_unk0", "color_id", "flags", "blend_id", "tex_id", "attributes", "s1_id", "s0_id", "s2_id", "_pad"},
			{}};
		out.rows.reserve(value.s4.size());
		for (const auto& item : value.s4) {
			out.rows.push_back(
				{cell::unum(item._unk0),
				 cell::unum(item.color_id),
				 cell::unum(item.flags),
				 cell::unum(item.blend_id),
				 cell::unum(item.tex_id),
				 cell::unum(item.attributes),
				 cell::unum(item.s1_id),
				 cell::unum(item.s0_id),
				 cell::unum(item.s2_id),
				 cell::unum(item._pad)});
		}
		tables.push_back(std::move(out));
	}

	{
		table out{"s5", section_labels[5], {"s3_id", "_unk0", "_unk1", "flags"}, {}};
		out.rows.reserve(value.s5.size());
		for (const auto& item : value.s5)
			out.rows.push_back(
				{cell::unum(item.s3_id), cell::unum(item._unk0), cell::unum(item._unk1), cell::unum(item.flags)});
		tables.push_back(std::move(out));
	}

	{
		table out{
			"s6",
			section_labels[6],
			{"left", "top", "right", "bottom", "s4_id", "s5_id", "s4_no", "s5_no", "flags", "_pad0"},
			{}};
		out.rows.reserve(value.s6.size());
		for (const auto& item : value.s6) {
			out.rows.push_back(
				{cell::real(item.left),
				 cell::real(item.top),
				 cell::real(item.right),
				 cell::real(item.bottom),
				 cell::unum(item.s4_id),
				 cell::unum(item.s5_id),
				 cell::unum(item.s4_no),
				 cell::unum(item.s5_no),
				 cell::unum(item.flags),
				 cell::unum(item._pad0)});
		}
		tables.push_back(std::move(out));
	}

	{
		table out{
			"s7",
			section_labels[7],
			{"move_x", "move_y", "move_z", "rotate_x", "rotate_y", "rotate_z", "scale_x", "scale_y", "fog"},
			{}};
		out.rows.reserve(value.s7.size());
		for (const auto& item : value.s7) {
			out.rows.push_back(
				{cell::real(item.move.x),
				 cell::real(item.move.y),
				 cell::real(item.move.z),
				 cell::real(item.rotate.x),
				 cell::real(item.rotate.y),
				 cell::real(item.rotate.z),
				 cell::real(item.scale.x),
				 cell::real(item.scale.y),
				 cell::unum(item.fog)});
		}
		tables.push_back(std::move(out));
	}

	{
		table out{
			"s8",
			section_labels[8],
			{"s6_id",	  "_pad0",		   "s7_id", "frames", "FLIPX",		"FLIPY",	   "JUMP",		  "0x20",
			 "0x80",	  "HITBOX",		   "LAST",	"0x2000", "loop_s8_id", "s5s3_interp", "interp_rate", "s7_interp",
			 "s6_interp", "s0s1s2_interp", "n_180", "_pad1",  "_pad2",		"sfx_mute",	   "sfx_id"},
			{}};
		out.rows.reserve(value.s8.size());
		for (const auto& item : value.s8) {
			out.rows.push_back(
				{cell::unum(item.s6_id),
				 cell::unum(item._pad0),
				 cell::unum(item.s7_id),
				 cell::unum(item.frames),
				 cell::unum((item.flags & 0x01) ? 1 : 0),
				 cell::unum((item.flags & 0x02) ? 1 : 0),
				 cell::unum((item.flags & 0x04) ? 1 : 0),
				 cell::unum((item.flags & 0x20) ? 1 : 0),
				 cell::unum((item.flags & 0x80) ? 1 : 0),
				 cell::unum((item.flags & 0x400) ? 1 : 0),
				 cell::unum((item.flags & 0x800) ? 1 : 0),
				 cell::unum((item.flags & 0x2000) ? 1 : 0),
				 cell::unum(item.loop_s8_id),
				 cell::unum(item.s5s3_interpolation),
				 cell::unum(item.interpolation_rate),
				 cell::unum(item.s7_interpolation),
				 cell::unum(item.s6_interpolation),
				 cell::unum(item.s0s1s2_interpolation),
				 cell::unum(item.n_180),
				 cell::unum(item._pad1),
				 cell::unum(item._pad2),
				 cell::unum(item.sfx_mute),
				 cell::unum(item.sfx_id)});
		}
		tables.push_back(std::move(out));
	}

	{
		table out{
			"s9",
			section_labels[9],
			{"left",
			 "top",
			 "right",
			 "bottom",
			 "name",
			 "sa_set_id",
			 "sa_set_no",
			 "sa_set_main",
			 "sa_sb_set_id",
			 "sa_sb_set_no",
			 "disabled"},
			{}};
		out.rows.reserve(value.s9.size());
		for (const auto& item : value.s9) {
			out.rows.push_back(
				{cell::real(item.left),
				 cell::real(item.top),
				 cell::real(item.right),
				 cell::real(item.bottom),
				 cell::str(fixed_name(item.name)),
				 cell::unum(item.sa_set_id),
				 cell::unum(item.sa_set_no),
				 cell::unum(item.sa_set_main),
				 cell::unum(item.sa_sb_set_id),
				 cell::unum(item.sa_sb_set_no),
				 cell::unum(item.disabled)});
		}
		tables.push_back(std::move(out));
	}

	{
		table out{
			"sa",
			section_labels[10],
			{"s8_id", "s8_no", "s8_sum", "s8_sum_once", "commit_ticks", "sb_id", "sb_no", "s8_st", "track_id", "_pad"},
			{}};
		out.rows.reserve(value.sa.size());
		for (const auto& item : value.sa) {
			out.rows.push_back(
				{cell::unum(item.s8_id),
				 cell::unum(item.s8_no),
				 cell::unum(item.s8_sum),
				 cell::snum(item.s8_sum_once),
				 cell::snum(item.commit_ticks),
				 cell::unum(item.sb_id),
				 cell::unum(item.sb_no),
				 cell::unum(item.s8_st),
				 cell::unum(item.track_id),
				 cell::unum(item._pad)});
		}
		tables.push_back(std::move(out));
	}

	{
		table out{
			"sb", section_labels[11], {"speed_num", "speed_den", "_pad0", "_pad1", "oneshot", "_pad2", "_pad3"}, {}};
		out.rows.reserve(value.sb.size());
		for (const auto& item : value.sb) {
			out.rows.push_back(
				{cell::unum(item.speed_num),
				 cell::unum(item.speed_den),
				 cell::unum(item._pad0),
				 cell::unum(item._pad1),
				 cell::unum(item.oneshot),
				 cell::unum(item._pad2),
				 cell::unum(item._pad3)});
		}
		tables.push_back(std::move(out));
	}

	return tables;
}

const table* find_table(const std::vector<table>& tables, std::string_view id) {
	for (const auto& item : tables)
		if (item.id == id)
			return &item;
	return nullptr;
}

size_t raw_row_limit(const options& opt, size_t rows) {
	if (opt.limit_set)
		return std::min(opt.limit, rows);
	return rows;
}

void write_table_text(const table& out, const options& opt) {
	const size_t shown = raw_row_limit(opt, out.rows.size());
	std::cout << "section " << out.id << " (" << out.label << ") rows=" << out.rows.size() << '\n';
	std::cout << "  columns:";
	for (auto column : out.columns)
		std::cout << ' ' << column;
	std::cout << '\n';
	for (size_t i = 0; i < shown; ++i) {
		std::cout << "  [" << i << "]";
		for (size_t j = 0; j < out.columns.size(); ++j) {
			std::cout << ' ' << out.columns[j] << '=';
			write_cell_text(out.rows[i][j]);
		}
		std::cout << '\n';
	}
	if (shown < out.rows.size())
		std::cout << "  ... " << (out.rows.size() - shown) << " more rows\n";
}

void write_table_json(json_writer& json, const table& out, const options& opt) {
	const size_t shown = raw_row_limit(opt, out.rows.size());
	json.begin_object(out.id);
	json.field("description", out.label);
	json.field("row_count", out.rows.size());
	json.begin_array("columns");
	for (auto column : out.columns)
		json.string(column);
	json.end_array();
	json.begin_array("rows");
	for (size_t i = 0; i < shown; ++i) {
		json.begin_array();
		for (const auto& value : out.rows[i])
			write_cell_json(json, value);
		json.end_array();
	}
	json.end_array();
	if (shown < out.rows.size())
		json.field("truncated", true);
	json.end_object();
}

bool valid_section(std::string_view section) {
	return std::find(section_ids.begin(), section_ids.end(), section) != section_ids.end();
}

size_t track_sequence_count(const mbs::v77& value, const mbs::section_9& track) {
	if (track.sa_set_id >= value.sa.size())
		return 0;
	return std::min<size_t>(track.sa_set_no, value.sa.size() - track.sa_set_id);
}

size_t sequence_frame_count(const mbs::v77& value, const mbs::section_a& sequence) {
	if (sequence.s8_id >= value.s8.size())
		return 0;
	return std::min<size_t>(sequence.s8_no, value.s8.size() - sequence.s8_id);
}

struct sequence_metrics {
	int64_t total_frames = 0;
	uint32_t loop_start = 0;
};

sequence_metrics measure_sequence(const mbs::v77& value, const mbs::section_a& sequence) {
	sequence_metrics metrics;
	uint32_t accumulated = 0;
	const size_t count = sequence_frame_count(value, sequence);
	for (size_t i = 0; i < count; ++i) {
		const auto frames = value.s8[sequence.s8_id + i].frames;
		metrics.total_frames += frames;
		accumulated += frames;
		if (sequence.s8_sum_once > 0 && metrics.loop_start == 0 &&
			accumulated >= static_cast<uint32_t>(sequence.s8_sum_once))
			metrics.loop_start = static_cast<uint32_t>(i + 1);
	}
	return metrics;
}

std::string flag_text(uint32_t flags) {
	std::string text;
	auto append = [&](uint32_t bit, std::string_view name) {
		if ((flags & bit) == 0)
			return;
		if (!text.empty())
			text += '|';
		text += name;
	};
	append(0x01, "FLIPX");
	append(0x02, "FLIPY");
	append(0x04, "JUMP");
	append(0x20, "ACTIVE");
	append(0x80, "POSE");
	append(0x400, "HITBOX");
	append(0x800, "LAST");
	append(0x2000, "0x2000");
	return text.empty() ? "none" : text;
}

void write_track_summary_text(const mbs::v77& value, size_t index) {
	const auto& track = value.s9[index];
	const size_t sequence_count = track_sequence_count(value, track);
	std::cout << "track[" << index << "] " << fixed_name(track.name) << " seqs=" << sequence_count
			  << " sa=" << track.sa_set_id << " bounds=(" << track.left << "," << track.top << "," << track.right << ","
			  << track.bottom << ") main=" << static_cast<unsigned>(track.sa_set_main)
			  << " disabled=" << static_cast<unsigned>(track.disabled) << '\n';
}

void dump_track_text(const mbs::v77& value, size_t index, const options& opt) {
	write_track_summary_text(value, index);
	const auto& track = value.s9[index];
	const size_t count = track_sequence_count(value, track);
	const size_t shown = opt.limit_set ? std::min(opt.limit, count) : count;
	for (size_t i = 0; i < shown; ++i) {
		const size_t sa_index = track.sa_set_id + i;
		const auto& sequence = value.sa[sa_index];
		const auto metrics = measure_sequence(value, sequence);
		std::cout << "  seq[" << i << "] sa=" << sa_index << " s8[" << sequence.s8_id << ".."
				  << (sequence.s8_id + sequence.s8_no) << ") total_frames=" << metrics.total_frames
				  << " once=" << sequence.s8_sum_once << " commit=" << sequence.commit_ticks
				  << " track=" << sequence.track_id << " loop=" << metrics.loop_start << '\n';
	}
	if (shown < count)
		std::cout << "  ... " << (count - shown) << " more sequences\n";
}

void dump_sequence_text(const mbs::v77& value, size_t track_index, size_t sequence_index, const options& opt) {
	write_track_summary_text(value, track_index);
	const auto& track = value.s9[track_index];
	const size_t sa_index = track.sa_set_id + sequence_index;
	const auto& sequence = value.sa[sa_index];
	const auto metrics = measure_sequence(value, sequence);
	const size_t count = sequence_frame_count(value, sequence);
	const size_t shown = opt.limit_set ? std::min(opt.limit, count) : count;

	std::cout << "sequence[" << sequence_index << "] sa=" << sa_index << " s8[" << sequence.s8_id << ".."
			  << (sequence.s8_id + sequence.s8_no) << ") total_frames=" << metrics.total_frames
			  << " once=" << sequence.s8_sum_once << " commit=" << sequence.commit_ticks
			  << " track=" << sequence.track_id << " loop=" << metrics.loop_start << '\n';
	for (size_t i = 0; i < shown; ++i) {
		const size_t s8_index = sequence.s8_id + i;
		const auto& frame = value.s8[s8_index];
		std::cout << "  frame[" << i << "] s8=" << s8_index << " duration=" << frame.frames
				  << " flags=" << std::format("{:#x}", frame.flags) << " [" << flag_text(frame.flags)
				  << "] s6=" << frame.s6_id << " s7=" << frame.s7_id << " loop_s8_id=" << frame.loop_s8_id
				  << " s5s3=" << static_cast<unsigned>(frame.s5s3_interpolation)
				  << " rate=" << static_cast<unsigned>(frame.interpolation_rate)
				  << " s7i=" << static_cast<unsigned>(frame.s7_interpolation)
				  << " s6i=" << static_cast<unsigned>(frame.s6_interpolation)
				  << " s0s1s2=" << static_cast<unsigned>(frame.s0s1s2_interpolation)
				  << " n_180=" << static_cast<unsigned>(frame.n_180) << " sfx=" << frame.sfx_id
				  << " mute=" << frame.sfx_mute << '\n';
	}
	if (shown < count)
		std::cout << "  ... " << (count - shown) << " more frames\n";
}

void write_track_json(json_writer& json, const mbs::v77& value, size_t index) {
	const auto& track = value.s9[index];
	json.begin_object();
	json.field("index", index);
	json.field("name", fixed_name(track.name));
	json.begin_array("bounds");
	json.number(track.left);
	json.number(track.top);
	json.number(track.right);
	json.number(track.bottom);
	json.end_array();
	json.field("sa_index", track.sa_set_id);
	json.field("sequence_count", track_sequence_count(value, track));
	json.field("sa_sb_index", track.sa_sb_set_id);
	json.field("sa_sb_count", track.sa_sb_set_no);
	json.field("main", track.sa_set_main);
	json.field("disabled", track.disabled != 0);
	json.end_object();
}

void write_sequence_json(json_writer& json, const mbs::v77& value, size_t track_index, size_t sequence_index) {
	const auto& track = value.s9[track_index];
	const size_t sa_index = track.sa_set_id + sequence_index;
	const auto& sequence = value.sa[sa_index];
	const auto metrics = measure_sequence(value, sequence);
	json.begin_object();
	json.field("sequence_index", sequence_index);
	json.field("sa_index", sa_index);
	json.field("s8_index", sequence.s8_id);
	json.field("s8_count", sequence.s8_no);
	json.field("s8_sum", sequence.s8_sum);
	json.field("s8_sum_once", sequence.s8_sum_once);
	json.field("commit_ticks", sequence.commit_ticks);
	json.field("sb_id", sequence.sb_id);
	json.field("sb_no", sequence.sb_no);
	json.field("s8_st", sequence.s8_st);
	json.field("track_id", sequence.track_id);
	json.field("total_frames", metrics.total_frames);
	json.field("loop_start", metrics.loop_start);
	json.end_object();
}

void write_frame_json(json_writer& json, const mbs::section_8& frame, size_t frame_index, size_t s8_index) {
	json.begin_object();
	json.field("frame_index", frame_index);
	json.field("s8_index", s8_index);
	json.field("s6_id", frame.s6_id);
	json.field("s7_id", frame.s7_id);
	json.field("duration", frame.frames);
	json.field("flags", frame.flags);
	json.field("loop_s8_id", frame.loop_s8_id);
	json.field("s5s3_interpolation", frame.s5s3_interpolation);
	json.field("interpolation_rate", frame.interpolation_rate);
	json.field("s7_interpolation", frame.s7_interpolation);
	json.field("s6_interpolation", frame.s6_interpolation);
	json.field("s0s1s2_interpolation", frame.s0s1s2_interpolation);
	json.field("n_180", frame.n_180);
	json.field("sfx_mute", frame.sfx_mute);
	json.field("sfx_id", frame.sfx_id);
	json.end_object();
}

void dump_track_query_json(const context& ctx, const mbs::v77& value, const mbs::file_header& header, size_t index) {
	const auto& track = value.s9[index];
	const size_t count = track_sequence_count(value, track);
	const size_t shown = ctx.opt.limit_set ? std::min(ctx.opt.limit, count) : count;

	json_writer json(std::cout);
	json.begin_object();
	write_metadata_json(json, ctx, "mbs");
	json.field("version", header.version);
	json.field("file_size", header.file_size);
	json.key("track");
	write_track_json(json, value, index);
	json.begin_array("sequences");
	for (size_t i = 0; i < shown; ++i)
		write_sequence_json(json, value, index, i);
	json.end_array();
	if (shown < count)
		json.field("sequences_truncated", true);
	json.end_object();
	std::cout << '\n';
}

void dump_sequence_query_json(
	const context& ctx,
	const mbs::v77& value,
	const mbs::file_header& header,
	size_t track_index,
	size_t sequence_index) {
	const auto& track = value.s9[track_index];
	const size_t sa_index = track.sa_set_id + sequence_index;
	const auto& sequence = value.sa[sa_index];
	const size_t count = sequence_frame_count(value, sequence);
	const size_t shown = ctx.opt.limit_set ? std::min(ctx.opt.limit, count) : count;

	json_writer json(std::cout);
	json.begin_object();
	write_metadata_json(json, ctx, "mbs");
	json.field("version", header.version);
	json.field("file_size", header.file_size);
	json.key("track");
	write_track_json(json, value, track_index);
	json.key("sequence");
	write_sequence_json(json, value, track_index, sequence_index);
	json.begin_array("frames");
	for (size_t i = 0; i < shown; ++i) {
		const size_t s8_index = sequence.s8_id + i;
		write_frame_json(json, value.s8[s8_index], i, s8_index);
	}
	json.end_array();
	if (shown < count)
		json.field("frames_truncated", true);
	json.end_object();
	std::cout << '\n';
}

} // namespace

int dump_mbs(const context& ctx, std::span<char> data) {
	if (data.size() < sizeof(mbs::file_header)) {
		std::cerr << "d: " << ctx.path << ": truncated MBS header\n";
		return 1;
	}

	mbs::file_header header{};
	std::memcpy(&header, data.data(), sizeof(header));
	if (std::memcmp(header.magic, "FMBS", 4) != 0) {
		std::cerr << "d: " << ctx.path << ": not an MBS file\n";
		return 1;
	}
	if (header.version != 0x76 && header.version != 0x77) {
		std::cerr << "d: " << ctx.path << ": unsupported MBS version\n";
		return 1;
	}

	const std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t*>(data.data()), data.size());
	mbs::v77 value;
	if (header.version == 0x77)
		mbs::parse_v77(bytes, value);
	else
		mbs::parse_v76(bytes, value);

	const std::array<size_t, 12> counts{
		value.s0.size(),
		value.s1.size(),
		value.s2.size(),
		value.s3.size(),
		value.s4.size(),
		value.s5.size(),
		value.s6.size(),
		value.s7.size(),
		value.s8.size(),
		value.s9.size(),
		value.sa.size(),
		value.sb.size()};

	if (!ctx.opt.section.empty() && !valid_section(ctx.opt.section)) {
		std::cerr << "d: " << ctx.path << ": unknown MBS section " << ctx.opt.section << '\n';
		return 2;
	}

	if (ctx.opt.track) {
		const size_t track_index = *ctx.opt.track;
		if (track_index >= value.s9.size()) {
			std::cerr << "d: " << ctx.path << ": track index out of range\n";
			return 2;
		}
		if (ctx.opt.sequence) {
			const size_t sequence_index = *ctx.opt.sequence;
			if (sequence_index >= track_sequence_count(value, value.s9[track_index])) {
				std::cerr << "d: " << ctx.path << ": sequence index out of range\n";
				return 2;
			}
			if (ctx.opt.json)
				dump_sequence_query_json(ctx, value, header, track_index, sequence_index);
			else
				dump_sequence_text(value, track_index, sequence_index, ctx.opt);
		} else {
			if (ctx.opt.json)
				dump_track_query_json(ctx, value, header, track_index);
			else
				dump_track_text(value, track_index, ctx.opt);
		}
		return 0;
	}

	if (ctx.opt.json) {
		const auto tables = build_tables(value);
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "mbs");
		json.field("version", header.version);
		json.field("file_size", header.file_size);
		json.field("header_size", header.header_size);
		json.begin_object("section_counts");
		for (size_t i = 0; i < counts.size(); ++i)
			json.field(section_ids[i], counts[i]);
		json.end_object();
		json.begin_object("sections");
		if (!ctx.opt.section.empty()) {
			const auto* selected = find_table(tables, ctx.opt.section);
			if (selected)
				write_table_json(json, *selected, ctx.opt);
		} else {
			for (const auto& table : tables)
				write_table_json(json, table, ctx.opt);
		}
		json.end_object();
		json.end_object();
		std::cout << '\n';
		return 0;
	}

	write_metadata_text(ctx, "mbs");
	std::cout << "version=" << std::format("{:#x}", header.version) << " (" << header.version
			  << ") file_size=" << header.file_size << " header_size=" << header.header_size << '\n';
	std::cout << "sections:";
	for (size_t i = 0; i < counts.size(); ++i)
		std::cout << ' ' << section_ids[i] << '=' << counts[i];
	std::cout << '\n';

	if (ctx.opt.section.empty() && !ctx.opt.full) {
		const size_t track_limit = ctx.opt.limit_set ? std::min(ctx.opt.limit, value.s9.size()) : value.s9.size();
		std::cout << "tracks (" << value.s9.size() << "):\n";
		for (size_t track_index = 0; track_index < track_limit; ++track_index) {
			const auto& track = value.s9[track_index];
			const size_t sequence_count = track_sequence_count(value, track);
			std::cout << "  [" << track_index << "] " << fixed_name(track.name) << " seqs=" << sequence_count;
			if (sequence_count == 1)
				std::cout << " [0]";
			else if (sequence_count > 1)
				std::cout << " [0.." << (sequence_count - 1) << "]";
			std::cout << " bounds=(" << track.left << "," << track.top << "," << track.right << "," << track.bottom
					  << ") main=" << static_cast<unsigned>(track.sa_set_main)
					  << " disabled=" << static_cast<unsigned>(track.disabled) << '\n';
		}
		if (track_limit < value.s9.size())
			std::cout << "  ... " << (value.s9.size() - track_limit) << " more tracks\n";
	}

	if (ctx.opt.full || !ctx.opt.section.empty()) {
		const auto tables = build_tables(value);
		if (!ctx.opt.section.empty()) {
			const auto* selected = find_table(tables, ctx.opt.section);
			if (selected)
				write_table_text(*selected, ctx.opt);
		} else {
			for (const auto& table : tables)
				write_table_text(table, ctx.opt);
		}
	}
	return 0;
}

} // namespace dump
