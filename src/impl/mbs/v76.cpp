#include "detail.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace mbs {

// Lookup table for v76.  Differs from v77 in two ways:
//   1. Count fields use 16-bit values for most entries (only s4 uses 32-bit)
//   2. The file-offset positions of count/pointer fields differ slightly
//
// Element counts are read at offsets [c] with width [cs] (2 or 4 bytes).
// Data-start pointers are always 4-byte uint32_t at offset [p].
// Row lengths [row_length] match v77 exactly — the section structs are
// byte-identical across v76 and v77.
namespace v76 {

constexpr auto table = std::array{
	detail::lookup_entry{0x54, 2, 0xb0, 0x18}, // s0  fog colors
	detail::lookup_entry{0x56, 2, 0xb8, 0x30}, // s1  texture UVs
	detail::lookup_entry{0x58, 2, 0xc0, 0x30}, // s2  vertices
	detail::lookup_entry{0x5a, 2, 0xc8, 0x50}, // s3  hitboxes
	detail::lookup_entry{0x50, 4, 0xd0, 0x14}, // s4  keyframe layers
	detail::lookup_entry{0x5c, 2, 0xd8, 0x08}, // s5  hitbox entries
	detail::lookup_entry{0x62, 2, 0xe0, 0x1c}, // s6  keyframes
	detail::lookup_entry{0x5e, 2, 0xe8, 0x24}, // s7  transforms
	detail::lookup_entry{0x60, 2, 0xf0, 0x20}, // s8  animation frames
	detail::lookup_entry{0x64, 2, 0xf8, 0x30}, // s9  tracks
	detail::lookup_entry{0x66, 2, 0x100, 0x18}, // sa  sequences
	detail::lookup_entry{0x6a, 2, 0x108, 0x14}, // sb  extras
};

} // namespace v76

void parse_v76(std::span<const uint8_t> data, v77& v) {
	auto tbl = v76::table;
	byte_reader r(data);
	detail::resolve_table(r, tbl);
	detail::populate_sections(r, v, tbl);
}

} // namespace mbs
