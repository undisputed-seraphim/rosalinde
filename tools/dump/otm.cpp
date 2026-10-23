#include "dump.hpp"

#include <impl/otm.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>

namespace dump {
namespace {

std::span<const uint8_t> as_bytes(std::span<char> data) {
	return {reinterpret_cast<const uint8_t*>(data.data()), data.size()};
}

size_t row_limit(const options& opt, size_t rows) {
	if (!opt.full && !opt.json)
		return 0;
	if (opt.limit_set)
		return std::min(opt.limit, rows);
	return rows;
}

template <typename Key>
void write_count_map(json_writer& json, const char* name, const std::map<Key, uint64_t>& counts) {
	json.begin_array(name);
	for (const auto& [value, count] : counts) {
		json.begin_object();
		json.field("value", value);
		json.field("count", count);
		json.end_object();
	}
	json.end_array();
}

int dump_otm_stats(const context& ctx, const otm::file& file) {
	size_t tracks_with_events = 0;
	size_t empty_tracks = 0;
	for (const auto& track : file.tracks) {
		if (track.event_count == 0)
			++empty_tracks;
		else
			++tracks_with_events;
	}

	std::map<uint32_t, uint64_t> event_kinds;
	std::map<uint32_t, uint64_t> event_flags;
	std::map<uint32_t, uint64_t> cue_types;
	std::map<uint32_t, uint64_t> cue_flags;
	std::map<int32_t, uint64_t> cue_param0;
	std::map<int32_t, uint64_t> cue_param1;
	std::set<uint32_t> cue_ids;
	uint32_t cue_id_min = 0;
	uint32_t cue_id_max = 0;
	bool have_cue_id = false;
	uint32_t frame_max = 0;
	uint32_t value_max = 0;

	for (const auto& event : file.events) {
		++event_kinds[event.kind];
		++event_flags[event.flags];
	}
	for (const auto& cue : file.cues) {
		++cue_types[cue.type];
		++cue_flags[cue.flags];
		++cue_param0[cue.param0];
		++cue_param1[cue.param1];
		cue_ids.insert(cue.cue_id);
		if (!have_cue_id || cue.cue_id < cue_id_min)
			cue_id_min = cue.cue_id;
		if (!have_cue_id || cue.cue_id > cue_id_max)
			cue_id_max = cue.cue_id;
		have_cue_id = true;
		frame_max = std::max(frame_max, static_cast<uint32_t>(cue.frame));
		value_max = std::max(value_max, static_cast<uint32_t>(cue.value));
	}

	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "otm");
		json.field("version", file.hdr.version);
		json.field("mbs_name", file.mbs_name);
		json.field("track_count", file.tracks.size());
		json.field("event_count", file.events.size());
		json.field("cue_count", file.cues.size());
		json.field("endian_flags", file.hdr.endian_flags);
		json.field("tracks_with_events", tracks_with_events);
		json.field("empty_tracks", empty_tracks);
		json.field("cue_id_count", cue_ids.size());
		json.field("cue_id_min", cue_id_min);
		json.field("cue_id_max", cue_id_max);
		json.field("frame_max", frame_max);
		json.field("value_max", value_max);
		write_count_map(json, "event_kinds", event_kinds);
		write_count_map(json, "event_flags", event_flags);
		write_count_map(json, "cue_types", cue_types);
		write_count_map(json, "cue_flags", cue_flags);
		write_count_map(json, "cue_param0", cue_param0);
		write_count_map(json, "cue_param1", cue_param1);
		json.end_object();
		std::cout << '\n';
		return 0;
	}

	write_metadata_text(ctx, "otm");
	std::cout << "version=" << file.hdr.version << " mbs=" << file.mbs_name << " tracks=" << file.tracks.size()
			  << " events=" << file.events.size() << " cues=" << file.cues.size() << " tracks_with_events="
			  << tracks_with_events << " empty_tracks=" << empty_tracks << " cue_ids=" << cue_ids.size() << '\n';
	return 0;
}

} // namespace

int dump_otm(const context& ctx, std::span<char> data) {
	auto parsed = otm::parse(as_bytes(data));
	if (!parsed) {
		std::cerr << "d: " << ctx.path << ": not a valid OTM file\n";
		return 1;
	}
	const auto& file = *parsed;
	if (ctx.opt.stats)
		return dump_otm_stats(ctx, file);
	const size_t event_limit = row_limit(ctx.opt, file.events.size());
	const size_t cue_limit = row_limit(ctx.opt, file.cues.size());
	const size_t track_limit = ctx.opt.limit_set ? std::min(ctx.opt.limit, file.tracks.size()) : file.tracks.size();

	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "otm");
		json.field("version", file.hdr.version);
		json.field("mbs_name", file.mbs_name);
		json.field("track_count", file.tracks.size());
		json.field("event_count", file.events.size());
		json.field("cue_count", file.cues.size());
		json.field("endian_flags", file.hdr.endian_flags);
		if (track_limit < file.tracks.size())
			json.field("tracks_truncated", true);
		json.begin_array("tracks");
		for (size_t i = 0; i < track_limit; ++i) {
			const auto& track = file.tracks[i];
			json.begin_object();
			json.field("index", i);
			json.field("event_first", track.event_first);
			json.field("event_count", track.event_count);
			json.end_object();
		}
		json.end_array();
		json.begin_array("events");
		for (size_t i = 0; i < event_limit; ++i) {
			const auto& event = file.events[i];
			json.begin_object();
			json.field("index", i);
			json.field("cue_index", event.cue_index);
			json.field("kind", event.kind);
			json.field("flags", event.flags);
			json.end_object();
		}
		json.end_array();
		json.begin_array("cues");
		for (size_t i = 0; i < cue_limit; ++i) {
			const auto& cue = file.cues[i];
			json.begin_object();
			json.field("index", i);
			json.field("frame", cue.frame);
			json.field("value", cue.value);
			json.field("cue_id", cue.cue_id);
			json.field("type", cue.type);
			json.field("param0", cue.param0);
			json.field("param1", cue.param1);
			json.field("flags", cue.flags);
			json.end_object();
		}
		json.end_array();
		json.end_object();
		std::cout << '\n';
		return 0;
	}

	write_metadata_text(ctx, "otm");
	std::cout << "version=" << file.hdr.version << " mbs=" << file.mbs_name << " tracks=" << file.tracks.size()
			  << " events=" << file.events.size() << " cues=" << file.cues.size() << '\n';
	std::cout << "tracks:\n";
	for (size_t i = 0; i < track_limit; ++i) {
		const auto& track = file.tracks[i];
		std::cout << "  [" << i << "] events[" << track.event_first << ".." << (track.event_first + track.event_count)
				  << ")\n";
	}
	if (track_limit < file.tracks.size())
		std::cout << "  ... " << (file.tracks.size() - track_limit) << " more tracks\n";

	if (ctx.opt.full) {
		std::cout << "events:\n";
		for (size_t i = 0; i < event_limit; ++i) {
			const auto& event = file.events[i];
			std::cout << "  [" << i << "] cue_index=" << event.cue_index << " kind=" << event.kind
					  << " flags=" << event.flags << '\n';
		}
		if (event_limit < file.events.size())
			std::cout << "  ... " << (file.events.size() - event_limit) << " more events\n";

		std::cout << "cues:\n";
		for (size_t i = 0; i < cue_limit; ++i) {
			const auto& cue = file.cues[i];
			std::cout << "  [" << i << "] frame=" << cue.frame << " value=" << cue.value << " cue_id=" << cue.cue_id
					  << " type=" << cue.type << " param0=" << cue.param0 << " param1=" << cue.param1
					  << " flags=" << cue.flags << '\n';
		}
		if (cue_limit < file.cues.size())
			std::cout << "  ... " << (file.cues.size() - cue_limit) << " more cues\n";
	}
	return 0;
}

} // namespace dump
