#pragma once

#include "otm.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <string_view>

namespace otm {

using namespace std::literals;

inline void dump_csv(const file& f, const std::filesystem::path& outdir) {
	std::filesystem::create_directories(outdir);

	{
		auto ofs = std::ofstream(outdir / "header.csv");
		ofs << "magic,data_size,header_size,flags,mbs_name,endian_flags,unknown0,version,"
			<< "track_count,event_count,cue_count,unknown1,unknown2,"
			<< "track_offset,event_offset,cue_offset\n";
		ofs << std::format(
			"{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n"sv,
			std::string_view(f.hdr.magic, 4),
			f.hdr.data_size,
			f.hdr.header_size,
			f.hdr.flags,
			f.mbs_name,
			f.hdr.endian_flags,
			f.hdr.unknown0,
			f.hdr.version,
			f.hdr.track_count,
			f.hdr.event_count,
			f.hdr.cue_count,
			f.hdr.unknown1,
			f.hdr.unknown2,
			f.hdr.track_offset,
			f.hdr.event_offset,
			f.hdr.cue_offset);
	}

	{
		auto ofs = std::ofstream(outdir / "tracks.csv");
		ofs << "index,event_first,event_count,event_end\n";
		for (size_t i = 0; i < f.tracks.size(); ++i) {
			const auto& t = f.tracks[i];
			ofs << std::format("{},{},{},{}\n"sv, i, t.event_first, t.event_count, t.event_first + t.event_count);
		}
	}

	{
		auto ofs = std::ofstream(outdir / "events.csv");
		ofs << "index,cue_index,kind,flags\n";
		for (size_t i = 0; i < f.events.size(); ++i) {
			const auto& e = f.events[i];
			ofs << std::format("{},{},{},{}\n"sv, i, e.cue_index, e.kind, e.flags);
		}
	}

	{
		auto ofs = std::ofstream(outdir / "cues.csv");
		ofs << "index,frame,value,cue_id,cue_id_hex,type,param0,param1,flags\n";
		for (size_t i = 0; i < f.cues.size(); ++i) {
			const auto& c = f.cues[i];
			ofs << std::format(
				"{},{},{},{},{:#x},{},{},{},{}\n"sv,
				i,
				c.frame,
				c.value,
				c.cue_id,
				c.cue_id,
				c.type,
				c.param0,
				c.param1,
				c.flags);
		}
	}
}

} // namespace otm
