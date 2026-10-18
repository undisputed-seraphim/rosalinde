#include "dump.hpp"

#include <impl/otm.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>

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

} // namespace

int dump_otm(const context& ctx, std::span<char> data) {
	auto parsed = otm::parse(as_bytes(data));
	if (!parsed) {
		std::cerr << "d: " << ctx.path << ": not a valid OTM file\n";
		return 1;
	}
	const auto& file = *parsed;
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
