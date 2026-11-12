#include <criware/acb.hpp>
#include <criware/endian_swap.hpp>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace criware {
namespace {

using utf::blob_ref;
using utf::get;

uint16_t read_be16(const uint8_t* p) {
	uint16_t val;
	std::memcpy(&val, p, 2);
	return swap_endian(val);
}

using RootSchema = decltype(schema::make(
	schema::col<"Version", uint32_t>(),
	schema::optional_col<"CueTable", blob_ref>(),
	schema::optional_col<"CueNameTable", blob_ref>(),
	schema::optional_col<"WaveformTable", blob_ref>(),
	schema::optional_col<"SynthTable", blob_ref>(),
	schema::optional_col<"TrackTable", blob_ref>(),
	schema::optional_col<"TrackEventTable", blob_ref>(),
	schema::optional_col<"SequenceTable", blob_ref>(),
	schema::optional_col<"BlockSequenceTable", blob_ref>(),
	schema::optional_col<"BlockTable", blob_ref>(),
	schema::optional_col<"WaveformExtensionDataTable", blob_ref>(),
	schema::optional_col<"StreamAwbHash", blob_ref>(),
	schema::optional_col<"AwbFile", blob_ref>()));

using CueTableSchema = decltype(schema::make(
	schema::col<"CueId", uint32_t>(),
	schema::optional_col<"ReferenceType", uint8_t>(),
	schema::optional_col<"ReferenceIndex", uint16_t>(),
	schema::optional_col<"Length", uint32_t>()));

using CueNameTableSchema = decltype(schema::make(
	schema::optional_col<"CueIndex", uint16_t>(),
	schema::optional_col<"CueName", std::string_view>()));

using WaveformTableSchema = decltype(schema::make(
	schema::col<"EncodeType", uint8_t>(),
	schema::col<"Streaming", uint8_t>(),
	schema::col<"NumSamples", uint32_t>(),
	schema::optional_col<"LoopFlag", uint8_t>(),
	schema::optional_col<"NumChannels", uint8_t>(),
	schema::optional_col<"SamplingRate", uint32_t>(),
	schema::optional_col<"MemoryAwbId", uint16_t>(),
	schema::optional_col<"StreamAwbId", uint16_t>(),
	schema::optional_col<"StreamAwbPortNo", uint16_t>(),
	schema::optional_col<"ExtensionData", uint16_t>(),
	schema::optional_col<"Id", uint16_t>()));

using SynthTableSchema =
	decltype(schema::make(schema::optional_col<"ReferenceItems", blob_ref>(), schema::optional_col<"Type", uint8_t>()));

using TrackTableSchema = decltype(schema::make(
	schema::optional_col<"EventIndex", uint16_t>(),
	schema::optional_col<"CommandIndex", uint16_t>()));

using TrackEventTableSchema = decltype(schema::make(schema::optional_col<"Command", blob_ref>()));

using SequenceTableSchema = decltype(schema::make(
	schema::optional_col<"NumTracks", uint16_t>(),
	schema::optional_col<"TrackIndex", blob_ref>(),
	schema::optional_col<"Type", uint8_t>()));

using BlockSequenceTableSchema = decltype(schema::make(
	schema::optional_col<"NumTracks", uint16_t>(),
	schema::optional_col<"TrackIndex", blob_ref>(),
	schema::optional_col<"NumBlocks", uint16_t>(),
	schema::optional_col<"BlockIndex", blob_ref>()));

using BlockTableSchema = decltype(schema::make(
	schema::optional_col<"NumTracks", uint16_t>(),
	schema::optional_col<"TrackIndex", blob_ref>()));

using WaveformExtensionDataSchema =
	decltype(schema::make(schema::optional_col<"LoopStart", uint32_t>(), schema::optional_col<"LoopEnd", uint32_t>()));

using StreamAwbHashSchema =
	decltype(schema::make(schema::optional_col<"Name", std::string_view>(), schema::optional_col<"Hash", blob_ref>()));

template <class Schema>
std::optional<utf::table<Schema>> parse_optional_table(std::span<const uint8_t> data) {
	if (data.empty())
		return std::nullopt;
	try {
		return utf::table<Schema>::parse(data);
	} catch (const std::exception&) {
		return std::nullopt;
	}
}

struct resolver {
	const utf::table<SynthTableSchema>* synth = nullptr;
	const utf::table<TrackTableSchema>* track = nullptr;
	const utf::table<TrackEventTableSchema>* track_event = nullptr;
	const utf::table<SequenceTableSchema>* sequence = nullptr;
	const utf::table<BlockSequenceTableSchema>* block_sequence = nullptr;
	const utf::table<BlockTableSchema>* block = nullptr;
	const std::vector<ACBWaveform>* waveforms = nullptr;
	std::vector<uint16_t> out;
	int depth = 0;

	void add_waveform(uint64_t index) {
		if (!waveforms || index >= waveforms->size())
			return;
		const auto value = static_cast<uint16_t>(index);
		if (std::find(out.begin(), out.end(), value) == out.end())
			out.push_back(value);
	}

	void resolve_waveform(uint64_t index) { add_waveform(index); }

	void resolve_synth(uint64_t index) {
		if (!synth || depth >= 4 || index >= synth->size())
			return;
		auto ref = get<"ReferenceItems">((*synth)[static_cast<size_t>(index)]);
		if (!ref || ref->size < 4)
			return;
		std::span<const uint8_t> items;
		try {
			items = synth->blob(*ref);
		} catch (const std::exception&) {
			return;
		}
		for (size_t i = 0; i + 4 <= items.size(); i += 4) {
			const uint16_t type = read_be16(items.data() + i);
			const uint16_t item = read_be16(items.data() + i + 2);
			if (type == 0)
				break;
			if (type == 1)
				resolve_waveform(item);
			else if (type == 2) {
				++depth;
				resolve_synth(item);
				--depth;
			} else if (type == 3) {
				++depth;
				resolve_sequence(item);
				--depth;
			}
		}
	}

	void resolve_sequence(uint64_t index) {
		if (!sequence || depth >= 4 || index >= sequence->size())
			return;
		const auto row = (*sequence)[static_cast<size_t>(index)];
		const auto count = get<"NumTracks">(row).value_or(0);
		auto ref = get<"TrackIndex">(row);
		if (!ref)
			return;
		std::span<const uint8_t> items;
		try {
			items = sequence->blob(*ref);
		} catch (const std::exception&) {
			return;
		}
		if (count * 2 > items.size())
			return;
		for (uint64_t i = 0; i < count; ++i) {
			++depth;
			resolve_track(read_be16(items.data() + i * 2));
			--depth;
		}
	}

	void resolve_track(uint64_t index) {
		if (!track || depth >= 4 || index >= track->size())
			return;
		const auto event = get<"EventIndex">((*track)[static_cast<size_t>(index)]).value_or(0xFFFF);
		if (event == 0xFFFF || !track_event || event >= track_event->size())
			return;
		auto ref = get<"Command">((*track_event)[static_cast<size_t>(event)]);
		if (!ref || ref->size == 0)
			return;
		std::span<const uint8_t> command;
		try {
			command = track_event->blob(*ref);
		} catch (const std::exception&) {
			return;
		}
		size_t pos = 0;
		while (pos + 3 <= command.size()) {
			const uint16_t code = read_be16(command.data() + pos);
			const uint8_t length = command[pos + 2];
			pos += 3;
			if (length > command.size() - pos)
				break;
			if ((code == 2000 || code == 2003 || code == 2004) && length >= 4) {
				const uint16_t type = read_be16(command.data() + pos);
				const uint16_t item = read_be16(command.data() + pos + 2);
				if (type == 1)
					resolve_waveform(item);
				else if (type == 2) {
					++depth;
					resolve_synth(item);
					--depth;
				} else if (type == 3) {
					++depth;
					resolve_sequence(item);
					--depth;
				}
			}
			pos += length;
		}
	}

	void resolve_block(uint64_t index) {
		if (!block || depth >= 4 || index >= block->size())
			return;
		const auto row = (*block)[static_cast<size_t>(index)];
		const auto count = get<"NumTracks">(row).value_or(0);
		auto ref = get<"TrackIndex">(row);
		if (!ref)
			return;
		std::span<const uint8_t> items;
		try {
			items = block->blob(*ref);
		} catch (const std::exception&) {
			return;
		}
		if (count * 2 > items.size())
			return;
		for (uint64_t i = 0; i < count; ++i) {
			++depth;
			resolve_track(read_be16(items.data() + i * 2));
			--depth;
		}
	}

	void resolve_block_sequence(uint64_t index) {
		if (!block_sequence || depth >= 4 || index >= block_sequence->size())
			return;
		const auto row = (*block_sequence)[static_cast<size_t>(index)];
		const auto track_count = get<"NumTracks">(row).value_or(0);
		if (auto ref = get<"TrackIndex">(row)) {
			std::span<const uint8_t> items;
			try {
				items = block_sequence->blob(*ref);
			} catch (const std::exception&) {
				items = {};
			}
			if (track_count * 2 <= items.size()) {
				for (uint64_t i = 0; i < track_count; ++i) {
					++depth;
					resolve_track(read_be16(items.data() + i * 2));
					--depth;
				}
			}
		}
		const auto block_count = get<"NumBlocks">(row).value_or(0);
		if (auto ref = get<"BlockIndex">(row)) {
			std::span<const uint8_t> items;
			try {
				items = block_sequence->blob(*ref);
			} catch (const std::exception&) {
				items = {};
			}
			if (block_count * 2 <= items.size()) {
				for (uint64_t i = 0; i < block_count; ++i) {
					++depth;
					resolve_block(read_be16(items.data() + i * 2));
					--depth;
				}
			}
		}
	}
};

} // namespace

ACB ACB::parse(std::span<const uint8_t> data) {
	ACB acb;
	acb._owned_data.assign(data.begin(), data.end());
	acb._raw_data = acb._owned_data;

	const auto root = utf::table<RootSchema>::parse(acb._raw_data);
	const auto root_row = root[0];
	acb._format_version = get<"Version">(root_row);
	if (auto ref = get<"AwbFile">(root_row))
		acb._awb_file = *ref;

	auto sub = [&](auto ref) -> std::span<const uint8_t> {
		if (!ref || ref->size == 0)
			return {};
		try {
			return root.blob(*ref);
		} catch (const std::exception&) {
			return {};
		}
	};

	const auto cue_sub = sub(get<"CueTable">(root_row));
	const auto cue_name_sub = sub(get<"CueNameTable">(root_row));
	const auto waveform_sub = sub(get<"WaveformTable">(root_row));
	const auto synth_sub = sub(get<"SynthTable">(root_row));
	const auto track_sub = sub(get<"TrackTable">(root_row));
	const auto track_event_sub = sub(get<"TrackEventTable">(root_row));
	const auto sequence_sub = sub(get<"SequenceTable">(root_row));
	const auto block_sequence_sub = sub(get<"BlockSequenceTable">(root_row));
	const auto block_sub = sub(get<"BlockTable">(root_row));
	const auto extension_sub = sub(get<"WaveformExtensionDataTable">(root_row));
	const auto stream_hash_sub = sub(get<"StreamAwbHash">(root_row));

	const auto cue_table = parse_optional_table<CueTableSchema>(cue_sub);
	const auto waveform_table = parse_optional_table<WaveformTableSchema>(waveform_sub);
	const auto synth_table = parse_optional_table<SynthTableSchema>(synth_sub);
	const auto track_table = parse_optional_table<TrackTableSchema>(track_sub);
	const auto track_event_table = parse_optional_table<TrackEventTableSchema>(track_event_sub);
	const auto sequence_table = parse_optional_table<SequenceTableSchema>(sequence_sub);
	const auto block_sequence_table = parse_optional_table<BlockSequenceTableSchema>(block_sequence_sub);
	const auto block_table = parse_optional_table<BlockTableSchema>(block_sub);
	const auto extension_table = parse_optional_table<WaveformExtensionDataSchema>(extension_sub);

	if (!cue_table || !waveform_table || !synth_table)
		return acb;

	struct extension_row {
		uint32_t start = 0;
		uint32_t end = 0;
	};
	std::vector<extension_row> extensions;
	if (extension_table) {
		extensions.reserve(extension_table->size());
		for (size_t i = 0; i < extension_table->size(); ++i) {
			const auto row = (*extension_table)[i];
			extensions.push_back({
				get<"LoopStart">(row).value_or(0),
				get<"LoopEnd">(row).value_or(0),
			});
		}
	}

	acb._waveforms.reserve(waveform_table->size());
	for (size_t i = 0; i < waveform_table->size(); ++i) {
		const auto row = (*waveform_table)[i];
		ACBWaveform waveform;
		waveform.waveform_index = static_cast<uint16_t>(i);
		waveform.encode_type = get<"EncodeType">(row);
		waveform.streaming = get<"Streaming">(row);
		waveform.loop_flag = get<"LoopFlag">(row).value_or(1);
		waveform.num_channels = get<"NumChannels">(row).value_or(1);
		waveform.sampling_rate = get<"SamplingRate">(row).value_or(48000);
		waveform.num_samples = get<"NumSamples">(row);
		waveform.memory_awb_id = get<"MemoryAwbId">(row).value_or(0xFFFF);
		waveform.stream_awb_id = get<"StreamAwbId">(row).value_or(0xFFFF);
		waveform.port = get<"StreamAwbPortNo">(row).value_or(0xFFFF);
		waveform.extension_index = get<"ExtensionData">(row).value_or(0xFFFF);

		if (auto id = get<"Id">(row))
			waveform.waveform_id = *id;
		else if (waveform.streaming != 0)
			waveform.waveform_id = waveform.stream_awb_id;
		else
			waveform.waveform_id = waveform.memory_awb_id;

		if (waveform.extension_index < extensions.size()) {
			waveform.loop_start = extensions[waveform.extension_index].start;
			waveform.loop_end = extensions[waveform.extension_index].end;
		}
		acb._waveforms.push_back(waveform);
	}

	resolver resol;
	resol.synth = synth_table ? &*synth_table : nullptr;
	resol.track = track_table ? &*track_table : nullptr;
	resol.track_event = track_event_table ? &*track_event_table : nullptr;
	resol.sequence = sequence_table ? &*sequence_table : nullptr;
	resol.block_sequence = block_sequence_table ? &*block_sequence_table : nullptr;
	resol.block = block_table ? &*block_table : nullptr;
	resol.waveforms = &acb._waveforms;

	acb._cues.reserve(cue_table->size());
	for (size_t i = 0; i < cue_table->size(); ++i) {
		const auto row = (*cue_table)[i];
		ACBCue cue;
		cue.cue_id = get<"CueId">(row);
		cue.reference_type = get<"ReferenceType">(row).value_or(0);
		cue.reference_index = get<"ReferenceIndex">(row).value_or(0);

		resol.out.clear();
		switch (cue.reference_type) {
		case 1:
			resol.resolve_waveform(cue.reference_index);
			break;
		case 2:
			resol.resolve_synth(cue.reference_index);
			break;
		case 3:
			resol.resolve_sequence(cue.reference_index);
			break;
		case 8:
			resol.resolve_block_sequence(cue.reference_index);
			break;
		default:
			break;
		}
		cue.waveform_indices = resol.out;

		if (auto length = get<"Length">(row); length && *length != 0xFFFFFFFF)
			cue.length = *length;

		for (uint16_t index : cue.waveform_indices) {
			const auto& waveform = acb._waveforms[index];
			if (cue.length == 0 || waveform.num_samples > cue.length)
				cue.length = waveform.num_samples;
			if (waveform.loop_flag == 2) {
				cue.loop = true;
				if (cue.loop_start == 0 && cue.loop_end == 0 && waveform.loop_end > waveform.loop_start) {
					cue.loop_start = waveform.loop_start;
					cue.loop_end = waveform.loop_end;
				}
			}
		}

		if (!cue.waveform_indices.empty()) {
			const auto& first = acb._waveforms[cue.waveform_indices.front()];
			cue.waveform_index = first.waveform_index;
			cue.waveform_id = first.waveform_id;
			cue.encode_type = first.encode_type;
			cue.streaming = first.streaming != 0;
			cue.waveform_identified = true;
		}
		acb._cues.push_back(std::move(cue));
	}

	if (const auto cue_name_table = parse_optional_table<CueNameTableSchema>(cue_name_sub)) {
		for (size_t i = 0; i < cue_name_table->size(); ++i) {
			const auto row = (*cue_name_table)[i];
			auto index = get<"CueIndex">(row);
			auto name = get<"CueName">(row);
			if (index && name && *index < acb._cues.size())
				acb._cues[*index].cue_name = std::string(*name);
		}
	}

	if (track_table) {
		acb._tracks.reserve(track_table->size());
		for (size_t i = 0; i < track_table->size(); ++i) {
			const auto row = (*track_table)[i];
			ACBTrack track;
			track.track_index = static_cast<uint32_t>(i);
			track.event_index = get<"EventIndex">(row).value_or(0xFFFF);

			resol.out.clear();
			resol.resolve_track(i);
			track.waveform_indices = resol.out;
			if (!track.waveform_indices.empty()) {
				const auto& first = acb._waveforms[track.waveform_indices.front()];
				track.waveform_index = first.waveform_index;
				track.waveform_id = first.waveform_id;
				track.encode_type = first.encode_type;
				track.streaming = first.streaming != 0;
				track.waveform_identified = true;
			}
			acb._tracks.push_back(std::move(track));
		}
	}

	if (const auto stream_hash = parse_optional_table<StreamAwbHashSchema>(stream_hash_sub)) {
		for (size_t i = 0; i < stream_hash->size(); ++i) {
			const auto row = (*stream_hash)[i];
			acb._stream_awb_names.emplace_back(get<"Name">(row).value_or(""));
		}
	}

	return acb;
}

const ACBCue* ACB::find_cue(uint32_t cue_id) const {
	for (const auto& cue : _cues)
		if (cue.cue_id == cue_id)
			return &cue;
	return nullptr;
}

const ACBTrack* ACB::find_track(uint32_t track_index) const {
	for (const auto& track : _tracks)
		if (track.track_index == track_index)
			return &track;
	return nullptr;
}

const ACBWaveform* ACB::find_waveform(uint16_t waveform_id) const {
	for (const auto& waveform : _waveforms)
		if (waveform.waveform_id == waveform_id)
			return &waveform;
	return nullptr;
}

const ACBWaveform* ACB::waveform_at(uint16_t waveform_index) const {
	return waveform_index < _waveforms.size() ? &_waveforms[waveform_index] : nullptr;
}

const std::string& ACB::stream_awb_name(uint16_t port) const {
	static const std::string empty;
	return port < _stream_awb_names.size() ? _stream_awb_names[port] : empty;
}

bool ACB::bind_external_awb(uint16_t port, std::span<const uint8_t> data, uint64_t archive_size) {
	auto parsed = AFS2::parse(data, 0, archive_size);
	if (parsed.entries().empty())
		return false;
	_external_awb_data[port] = data;
	_external_awb_sizes[port] = archive_size;
	_external_awbs[port] = std::move(parsed);
	return true;
}

const AFS2* ACB::external_awb(uint16_t port) const {
	const auto it = _external_awbs.find(port);
	return it == _external_awbs.end() || !it->second ? nullptr : &*it->second;
}

std::optional<ACBWaveformLocation> ACB::locate_waveform(const ACBWaveform& waveform) const {
	ACBWaveformLocation location;
	location.port = waveform.port == 0xFFFF ? 0 : waveform.port;
	location.waveform_id = waveform.waveform_id;
	if (waveform.streaming == 0) {
		const auto* awb = internal_awb();
		if (!awb)
			return std::nullopt;
		const auto* entry = awb->find(waveform.waveform_id);
		if (!entry)
			return std::nullopt;
		location.internal = true;
		location.offset = entry->aligned;
		location.size = entry->size;
		return location;
	}

	const auto* awb = external_awb(location.port);
	if (!awb)
		return std::nullopt;
	const auto* entry = awb->find(waveform.waveform_id);
	if (!entry)
		return std::nullopt;
	location.internal = false;
	location.offset = entry->aligned;
	location.size = entry->size;
	return location;
}

std::optional<ACBWaveformLocation> ACB::locate_waveform(uint16_t waveform_index) const {
	const auto* waveform = waveform_at(waveform_index);
	if (!waveform)
		return std::nullopt;
	return locate_waveform(*waveform);
}

const AFS2* ACB::internal_awb() const {
	if (_internal_awb_cache)
		return &*_internal_awb_cache;
	if (_raw_data.empty() || _awb_file.size == 0)
		return nullptr;
	const uint64_t end = _awb_file.offset + _awb_file.size;
	_internal_awb_cache = AFS2::parse(_raw_data.subspan(_awb_file.offset, _awb_file.size), _awb_file.offset, end);
	return &*_internal_awb_cache;
}

bool ACB::extract_waveform(uint16_t waveform_id, std::vector<char>& out) const {
	const auto* waveform = find_waveform(waveform_id);
	if (!waveform)
		return false;
	const auto location = locate_waveform(*waveform);
	if (!location)
		return false;

	const uint8_t* source = nullptr;
	if (location->internal) {
		source = _raw_data.data() + location->offset;
	} else {
		const auto it = _external_awb_data.find(location->port);
		if (it == _external_awb_data.end())
			return false;
		if (location->offset > it->second.size() || location->size > it->second.size() - location->offset)
			return false;
		source = it->second.data() + location->offset;
	}
	out.resize(static_cast<size_t>(location->size));
	std::memcpy(out.data(), source, static_cast<size_t>(location->size));
	return true;
}

std::span<const uint8_t> ACB::waveform_span(uint16_t waveform_id) const {
	const auto* waveform = find_waveform(waveform_id);
	if (!waveform || waveform->streaming != 0)
		return {};

	const auto* awb = internal_awb();
	if (!awb)
		return {};

	const auto* entry = awb->find(waveform->waveform_id);
	if (!entry)
		return {};

	return _raw_data.subspan(entry->aligned, entry->size);
}

} // namespace criware
