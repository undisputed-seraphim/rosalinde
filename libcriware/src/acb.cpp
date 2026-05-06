#include <criware/acb.hpp>
#include <criware/byte_reader.hpp>
#include <criware/endian_swap.hpp>

#include <cassert>
#include <span>
#include <stdexcept>

namespace criware {
namespace {

using RootSchema = decltype(schema::make(
	schema::col<"Version", uint32_t>(),
	schema::col<"CueTable", UTF::field::data_t>(),
	schema::col<"CueNameTable", UTF::field::data_t>(),
	schema::col<"WaveformTable", UTF::field::data_t>(),
	schema::col<"SynthTable", UTF::field::data_t>(),
	schema::col<"TrackTable", UTF::field::data_t>(),
	schema::col<"SequenceTable", UTF::field::data_t>(),
	schema::col<"AwbFile", UTF::field::data_t>(),
	schema::col<"StreamAwbAfs2Header", UTF::field::data_t>()));

using CueTableSchema = decltype(schema::make(
	schema::col<"CueId", uint32_t>(),
	schema::col<"ReferenceType", uint8_t>(),
	schema::col<"ReferenceIndex", uint16_t>()));

using WaveformTableSchema = decltype(schema::make(
	schema::col<"EncodeType", uint8_t>(),
	schema::col<"Streaming", uint8_t>(),
	schema::col<"LoopFlag", uint8_t>(),
	schema::col<"NumChannels", uint8_t>(),
	schema::col<"SamplingRate", uint32_t>(),
	schema::col<"NumSamples", uint32_t>(),
	schema::col<"MemoryAwbId", uint16_t>(),
	schema::col<"StreamAwbId", uint16_t>()));

using SynthTableSchema = decltype(schema::make(
	schema::col<"ReferenceItems", UTF::field::data_t>(),
	schema::col<"Type", uint8_t>()));

using TrackTableSchema = decltype(schema::make(
	schema::col<"Scope", uint8_t>(),
	schema::col<"TargetType", uint8_t>(),
	schema::col<"TargetId", uint32_t>()));

using CueNameTableSchema = decltype(schema::make(
	schema::col<"CueIndex", uint16_t>(),
	schema::col<"CueName", std::string>()));

static uint16_t read_be16(const uint8_t* p) {
	uint16_t val;
	std::memcpy(&val, p, 2);
	return swap_endian(val);
}

static ACBWaveform build_waveform(const row_view<WaveformTableSchema>& row) {
	ACBWaveform w;
	w.encode_type   = get<"EncodeType">(row);
	w.streaming     = get<"Streaming">(row);
	w.loop_flag     = get<"LoopFlag">(row);
	w.num_channels  = get<"NumChannels">(row);
	w.sampling_rate = get<"SamplingRate">(row);
	w.num_samples   = get<"NumSamples">(row);
	w.memory_awb_id = get<"MemoryAwbId">(row);
	w.stream_awb_id = get<"StreamAwbId">(row);
	w.waveform_id   = w.streaming ? w.stream_awb_id : w.memory_awb_id;
	return w;
}

static void resolve_waveform(ACBCue& cue, const ACBWaveform& w) {
	cue.waveform_id = w.waveform_id;
	cue.encode_type = w.encode_type;
	cue.streaming = w.streaming;
	cue.waveform_identified = true;
}

static void resolve_waveform(ACBTrack& track, const ACBWaveform& w) {
	track.waveform_id = w.waveform_id;
	track.encode_type = w.encode_type;
	track.streaming = w.streaming;
	track.waveform_identified = true;
}

} // anonymous namespace

ACB ACB::parse(std::span<const uint8_t> data) {
	ACB acb;
	auto root = table<RootSchema>::parse(data);

	auto& version = root.column<"Version">();
	acb._format_version = version.size() > 0 ? version[0] : 0;

	auto& cueCol  = root.column<"CueTable">();
	auto& cnCol   = root.column<"CueNameTable">();
	auto& wvCol   = root.column<"WaveformTable">();
	auto& stCol   = root.column<"SynthTable">();
	auto& tkCol   = root.column<"TrackTable">();

	auto sub = [&](const std::vector<UTF::field::data_t>& refs) -> std::span<const uint8_t> {
		if (refs.empty()) return {};
		return data.subspan(refs[0].offset, refs[0].size);
	};

	auto cueSub = sub(cueCol);
	auto wvSub  = sub(wvCol);
	auto stSub  = sub(stCol);
	auto tkSub  = sub(tkCol);
	auto cnSub  = sub(cnCol);

	if (cueSub.empty() || wvSub.empty() || stSub.empty())
		return acb;

	uint64_t stBase = stCol[0].offset;

	auto cueTable = table<CueTableSchema>::parse(cueSub);
	auto waveformTable = table<WaveformTableSchema>::parse(wvSub);
	auto synthTable = table<SynthTableSchema>::parse(stSub);

	auto& cue_ids   = cueTable.column<"CueId">();
	auto& cue_rt    = cueTable.column<"ReferenceType">();
	auto& cue_ri    = cueTable.column<"ReferenceIndex">();
	bool rt_default = (cue_rt.size() == 1 && cue_ids.size() > 1);

	for (size_t i = 0; i < waveformTable.size(); ++i)
		acb._waveforms.push_back(build_waveform(waveformTable[i]));

	uint64_t ref_offset = 0;
	uint32_t ref_size = 0;
	uint32_t ref_correction = 0;

	for (size_t i = 0; i < cue_ids.size(); ++i) {
		ACBCue cue;
		cue.cue_id          = cue_ids[i];
		cue.reference_type  = rt_default ? cue_rt[0] : cue_rt[i];
		cue.reference_index = cue_ri[i];

		uint16_t waveform_index = 0;
		bool has_waveform = false;

		if (cue.reference_index < synthTable.size()) {
			auto synth_row = synthTable[cue.reference_index];
			auto& ref_items = get<"ReferenceItems">(synth_row);

			switch (cue.reference_type) {
			case 2:
				ref_offset = ref_items.offset;
				ref_size = ref_items.size;
				ref_correction = ref_size + 2;
				break;
			case 3:
			case 8:
				if (i == 0) {
					ref_offset = ref_items.offset;
					ref_size = ref_items.size;
					ref_correction = ref_size - 2;
				} else {
					ref_correction += 4;
				}
				break;
			default:
				break;
			}

			if (ref_size > 0 && ref_offset + ref_correction + 2 <= data.size()) {
				waveform_index = read_be16(data.data() + stBase + ref_offset + ref_correction);
				cue.waveform_index = waveform_index;
				has_waveform = true;
			}
		}

		if (has_waveform && waveform_index < acb._waveforms.size())
			resolve_waveform(cue, acb._waveforms[waveform_index]);

		acb._cues.push_back(std::move(cue));
	}

	if (!tkSub.empty() && !stSub.empty()) {
		auto trackTable = table<TrackTableSchema>::parse(tkSub);

		for (size_t i = 0; i < trackTable.size(); ++i) {
			ACBTrack track;
			track.track_index = static_cast<uint32_t>(i);
			track.synth_index = track.track_index;

			uint16_t waveform_index = 0;
			bool has_waveform = false;

			if (track.synth_index < synthTable.size()) {
				auto synth_row = synthTable[track.synth_index];
				auto& ref_items = get<"ReferenceItems">(synth_row);

			if (ref_items.size > 0 && ref_items.offset + ref_items.size - 2 + 2 <= data.size()) {
				waveform_index = read_be16(data.data() + stBase + ref_items.offset + ref_items.size - 2);
				track.waveform_index = waveform_index;
					has_waveform = true;
				}
			}

			if (has_waveform && waveform_index < acb._waveforms.size())
				resolve_waveform(track, acb._waveforms[waveform_index]);

			acb._tracks.push_back(std::move(track));
		}
	}

	if (!cnSub.empty()) {
		auto cueNameTable = table<CueNameTableSchema>::parse(cnSub);
		for (size_t i = 0; i < cueNameTable.size(); ++i) {
			auto row = cueNameTable[i];
			uint16_t cue_idx = get<"CueIndex">(row);
			if (cue_idx < acb._cues.size())
				acb._cues[cue_idx].cue_name = get<"CueName">(row);
		}
	}

	return acb;
}

const ACBCue* ACB::find_cue(uint32_t cue_id) const {
	for (auto& c : _cues)
		if (c.cue_id == cue_id) return &c;
	return nullptr;
}

const ACBTrack* ACB::find_track(uint32_t track_index) const {
	for (auto& t : _tracks)
		if (t.track_index == track_index) return &t;
	return nullptr;
}

} // namespace criware
