#include <criware/afs2.hpp>
#include <criware/byte_reader.hpp>

#include <cassert>
#include <cstring>
#include <stdexcept>

namespace criware {

// ============================================================================
// AFS2 binary layout (derived via libcgss cross-reference + hex analysis):
//
// ALL multibyte fields are LITTLE-ENDIAN (unlike @UTF which is BE).
// This is confirmed by testing: BE interpretation gives nonsensical values
// (e.g. fileCount = 16 million instead of 1 for voice_add_jp.acb).
//
//   Offset  Size  Field
//   0x00    4     magic "AFS2"
//   0x04    4     version (uint32 LE)
//                   offset_field_size = (version >> 8) & 0xFF
//                   id_field_size     = version & 0xFF       (always 2)
//   0x08    4     file_count (int32 LE)
//   0x0C    4     byte_alignment (uint32 LE)
//                   alignment = lower 16 bits
//                   hca_key_modifier = upper 16 bits
//   0x10    N*S   entry array (S = offset_field_size bytes per entry)
//                   Each entry: uint16 LE id at offset 0
//                   (remaining S-2 bytes are padding/zeros)
//   0x10+N*S  N*S offset array (same stride S, same count N)
//                   Each offset: S-byte LE integer, masked to S bytes
//
// File data for entry i starts at entries[i].offset (absolute in stream).
// Size = next entry offset - this entry offset (or archive_end - this for last).
// ============================================================================

namespace {

template <typename T>
static T read_le(const uint8_t* p) {
	static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8,
		"read_le only supports 1/2/4/8 byte types");
	if constexpr (sizeof(T) == 1) {
		return *p;
	} else {
		T val = 0;
		for (size_t i = 0; i < sizeof(T); ++i)
			val |= static_cast<T>(p[i]) << (i * 8);
		return val;
	}
}

static uint64_t round_up(uint64_t value, uint64_t alignment) {
	if (alignment == 0) return value;
	return (value + alignment - 1) / alignment * alignment;
}

} // anonymous namespace

AFS2 AFS2::parse(std::span<const uint8_t> data, uint64_t base_offset) {
	AFS2 afs2;

	if (data.size() < 16) return afs2;

	const char magic_exp[] = {'A', 'F', 'S', '2'};
	if (std::memcmp(data.data(), magic_exp, 4) != 0) return afs2;

	// --- header ---
	const uint32_t raw_version  = read_le<uint32_t>(data.data() + 4);
	const int32_t  file_count   = static_cast<int32_t>(read_le<uint32_t>(data.data() + 8));
	const uint32_t raw_align    = read_le<uint32_t>(data.data() + 12);

	const uint32_t offset_field_size = (raw_version >> 8) & 0xFF;
	const uint32_t id_field_size     = raw_version & 0xFF;

	afs2._version   = raw_version;
	afs2._alignment = raw_align & 0xFFFF;
	afs2._hca_key   = static_cast<uint16_t>(raw_align >> 16);

	if (file_count <= 0)
		return afs2;

	const uint32_t entry_stride = offset_field_size; // ID + offset arrays share this stride
	if (entry_stride < 2 || entry_stride > 8)
		return afs2;

	// Build offset mask for offset_field_size bytes
	uint64_t offset_mask = 0;
	for (uint32_t i = 0; i < offset_field_size && i < 8; ++i)
		offset_mask |= static_cast<uint64_t>(0xFF) << (i * 8);

	afs2._entries.resize(static_cast<size_t>(file_count));

	// --- ID array (uint16 LE at start of each entry_stride-byte slot) ---
	const uint8_t* id_base = data.data() + 0x10;
	for (int32_t i = 0; i < file_count; ++i) {
		afs2._entries[i].id = read_le<uint16_t>(id_base + static_cast<size_t>(i) * entry_stride);
	}

	// --- offset array (starts after the ID entries: same stride, same count) ---
	const uint8_t* off_base = data.data() + 0x10 + static_cast<size_t>(file_count) * entry_stride;
	for (int32_t i = 0; i < file_count; ++i) {
		const uint8_t* p = off_base + static_cast<size_t>(i) * entry_stride;
		uint64_t raw_off = 0;
		for (uint32_t b = 0; b < offset_field_size && b < 8; ++b)
			raw_off |= static_cast<uint64_t>(p[b]) << (b * 8);
		raw_off &= offset_mask;

		afs2._entries[i].offset = raw_off + base_offset;
		afs2._entries[i].aligned = round_up(afs2._entries[i].offset, afs2._alignment);
	}

	// --- compute sizes (next.offset - this.offset) ---
	for (int32_t i = 0; i < file_count - 1; ++i) {
		afs2._entries[i].size = afs2._entries[i + 1].offset - afs2._entries[i].offset;
	}
	// Last entry: data goes to end of archive
	afs2._entries[static_cast<size_t>(file_count) - 1].size =
		static_cast<uint64_t>(data.size()) + base_offset -
		afs2._entries[static_cast<size_t>(file_count) - 1].offset;

	return afs2;
}

const AFS2::Entry* AFS2::find(uint16_t id) const {
	for (auto& e : _entries)
		if (e.id == id) return &e;
	return nullptr;
}

} // namespace criware
