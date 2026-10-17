#pragma once

#include <cstdint>
#include <istream>
#include <optional>
#include <span>
#include <string>
#include <vector>

// OTBM container (Criware ".otm", Unicorn Overlord). All multi-byte fields are
// little endian. The 120-byte header is followed by three offset-delimited
// arrays:
//
//   0x00  char[4]  "OTBM"
//   0x04  u32      data_size   = file size - 120
//   0x08  u32      header_size = 120
//   0x0c  u32      flags       (always 0)
//   0x10  char[64] MBS name, NUL padded
//   0x50  u16      endian_flags (bit0 = big endian, bit1 = relocated)
//   0x52  u16      unknown (0)
//   0x54  u16      version (always 6)
//   0x56  u16      track_count
//   0x58  u16      event_count
//   0x5a  u16      cue_count
//   0x5c  u16      unknown (0)
//   0x5e  u16      unknown (0)
//   0x60  u64      track_offset (120)
//   0x68  u64      event_offset
//   0x70  u64      cue_offset
//
// track_count * 4 bytes at track_offset, event_count * 4 bytes at event_offset,
// cue_count * 16 bytes at cue_offset. The file is padded with zeroes to a
// 16-byte boundary after the cue array.
//
// Verified against the 1500 OTMs in Unicorn.CPK:
//
//   - track_count equals the referenced MBS's section_9 (track) count in all
//     1500 files, and track i corresponds to MBS track i. Tracks without audio
//     have a zero-length event range.
//   - An event is one audio trigger inside a track. event.cue_index selects the
//     cue record; event.kind and event.flags are not decoded yet.
//   - cue.cue_id is a CRI ACB cue ID. 4065 of the 4068 distinct IDs in the
//     corpus resolve to Sound/se.acb or Sound/voice_btl_jp.acb. The three
//     misses are 0 (null) and two packed values in Gladiator_M / Soldier_M
//     (0xa80f81, 0xa85daa).
//   - cue.frame is a timeline frame inside the track. All 188695 character
//     events satisfy frame < the track's total section_8 frame duration, and
//     1817 of them are beyond the track's section_8 row count, so it is a
//     frame offset rather than a keyframe-row index.
//   - cue.value is always a multiple of 0x100 (Q8). Voice cues usually use 0
//     and SE cues use 10..117; this looks like a volume override (0 = default)
//     but is not confirmed.
//   - cue.type, cue.param0, cue.param1 and cue.flags are not decoded yet.
//     type is 0 for every character cue; GUI/BG/effect files also use 1, 4, 5.
//   - _MOBM1..6_[1..3] / _MOBF1..6_[1..3] are the 18 male/female voice-set
//     variants, not body-part meshes. Their maximum voice cue IDs follow:
//       male   1110045 + (style - 1) * 600 + (branch - 1) * 100
//       female 1110345 + (style - 1) * 600 + (branch - 1) * 100
//   - OTM is an audio timeline, not a colour/appearance table. Colour selection
//     lives in the MBS layer data.
namespace otm {

#pragma pack(push, 1)
struct header {
	char magic[4];
	uint32_t data_size;
	uint32_t header_size;
	uint32_t flags;
	char mbs_name[64];
	uint16_t endian_flags;
	uint16_t unknown0;
	uint16_t version;
	uint16_t track_count;
	uint16_t event_count;
	uint16_t cue_count;
	uint16_t unknown1;
	uint16_t unknown2;
	uint64_t track_offset;
	uint64_t event_offset;
	uint64_t cue_offset;
};

struct track {
	uint16_t event_first;
	uint16_t event_count;
};

struct event {
	uint16_t cue_index;
	uint8_t kind;
	uint8_t flags;
};

struct cue {
	uint16_t frame;
	uint16_t value;
	uint32_t cue_id;
	uint16_t type;
	int16_t param0;
	int16_t param1;
	uint16_t flags;
};
#pragma pack(pop)

static_assert(sizeof(header) == 120);
static_assert(sizeof(track) == 4);
static_assert(sizeof(event) == 4);
static_assert(sizeof(cue) == 16);

struct file {
	header hdr{};
	std::string mbs_name;
	std::vector<track> tracks;
	std::vector<event> events;
	std::vector<cue> cues;
};

std::optional<file> parse(std::span<const uint8_t> data);
std::optional<file> parse(std::istream& is);
std::optional<file> parse(std::istream&& is);

} // namespace otm
