#include "otm.hpp"

#include <criware/byte_reader.hpp>

#include <cstring>

namespace otm {

namespace {

constexpr uint32_t expected_header_size = 120;
constexpr uint16_t expected_version = 6;

} // namespace

std::optional<file> parse(std::span<const uint8_t> data) {
	if (data.size() < sizeof(header))
		return std::nullopt;

	byte_reader r(data);
	file f;
	f.hdr = r.read<header>();

	if (std::memcmp(f.hdr.magic, "OTBM", 4) != 0)
		return std::nullopt;
	if (f.hdr.header_size != expected_header_size)
		return std::nullopt;
	if (f.hdr.version != expected_version)
		return std::nullopt;
	if (f.hdr.data_size != data.size() - sizeof(header))
		return std::nullopt;
	// Big-endian files exist in the engine's loader but not in the CPK; the
	// engine's relocation of them is mixed-endian and not implemented here.
	if (f.hdr.endian_flags & 1)
		return std::nullopt;

	const size_t track_offset = f.hdr.track_offset;
	const size_t event_offset = f.hdr.event_offset;
	const size_t cue_offset = f.hdr.cue_offset;

	if (track_offset != sizeof(header))
		return std::nullopt;
	if (track_offset > event_offset || event_offset > cue_offset || cue_offset > data.size())
		return std::nullopt;
	if (event_offset - track_offset != size_t{f.hdr.track_count} * sizeof(track))
		return std::nullopt;
	if (cue_offset - event_offset != size_t{f.hdr.event_count} * sizeof(event))
		return std::nullopt;

	const size_t cue_bytes = size_t{f.hdr.cue_count} * sizeof(cue);
	if (data.size() - cue_offset < cue_bytes)
		return std::nullopt;
	if (data.size() - cue_offset - cue_bytes >= sizeof(cue))
		return std::nullopt;

	size_t name_len = 0;
	while (name_len < sizeof(f.hdr.mbs_name) && f.hdr.mbs_name[name_len] != '\0')
		++name_len;
	f.mbs_name.assign(f.hdr.mbs_name, name_len);

	r.seek(track_offset);
	f.tracks.resize(f.hdr.track_count);
	for (auto& t : f.tracks) {
		t.event_first = r.read<uint16_t>();
		t.event_count = r.read<uint16_t>();
	}

	r.seek(event_offset);
	f.events.resize(f.hdr.event_count);
	for (auto& e : f.events) {
		e.cue_index = r.read<uint16_t>();
		e.kind = r.read<uint8_t>();
		e.flags = r.read<uint8_t>();
	}

	r.seek(cue_offset);
	f.cues.resize(f.hdr.cue_count);
	for (auto& c : f.cues) {
		c.frame = r.read<uint16_t>();
		c.value = r.read<uint16_t>();
		c.cue_id = r.read<uint32_t>();
		c.type = r.read<uint16_t>();
		c.param0 = r.read<int16_t>();
		c.param1 = r.read<int16_t>();
		c.flags = r.read<uint16_t>();
	}

	for (const auto& t : f.tracks) {
		if (size_t{t.event_first} + t.event_count > f.events.size())
			return std::nullopt;
	}
	for (const auto& e : f.events) {
		if (e.cue_index >= f.cues.size() && !(f.cues.empty() && e.cue_index == 0))
			return std::nullopt;
	}

	return f;
}

std::optional<file> parse(std::istream& is) {
	std::vector<uint8_t> buffer;
	char c;
	while (is.get(c))
		buffer.push_back(static_cast<uint8_t>(c));
	return parse(std::span<const uint8_t>(buffer.data(), buffer.size()));
}

std::optional<file> parse(std::istream&& is) { return parse(is); }

} // namespace otm
