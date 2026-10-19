#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// MPD field mesh (FieldBG/*.mpd). All multi-byte fields are little endian.
//
// MPD is the static, textured quad mesh for a field background layer: FTX
// supplies the texture atlas, MBS supplies animation/track content for layers
// that need it, and MPD supplies the base ground geometry. The file is a
// 0x20-byte header followed by fixed 0xA0-byte quad records.
//
//   0x00  char[4] "MPD\0"
//   0x04  u32     data_size       file size - 0x20
//   0x08  u32     header_size     0x20
//   0x0c  u32     flags           0
//   0x10  u32     version         1
//   0x14  u32     record_count
//   0x18  u32     reserved        0
//   0x1c  u32     reserved        0
//
// Each record is four vertices (a quad in 0,1,2,3 order) plus a numeric name:
//
//   0x00  u32     id             numeric tile id; record.name is its decimal text
//   0x04  u32     texture_flags  low byte = FTX texture index; bit 16 also used
//   0x08  vertex[4]              4 * 0x18 bytes
//   0x68  char[56] name           NUL-terminated ASCII; max 4 chars in corpus
//
// vertex:
//
//   0x00  f32 x, y, z
//   0x0c  u32 color             0xffffffff in every corpus vertex
//   0x10  f32 u, v               normalized FTX texture coordinates
//
// Verified against 25 FieldBG MPDs (3,759 records): data_size, header_size and
// record_count consume every byte exactly. IDs and names match 100%; the 1,570
// unique ids are decimal strings. Texture indices are 0..9 and every one is
// below the entry count of the same-stem FTX. UVs are in [0,1], z is zero
// everywhere, and every color is opaque white. The high bit 0x10000 appears in
// texture_flags on some records; its meaning is not decoded.
namespace mpd {

#pragma pack(push, 1)
struct header {
	char magic[4];
	uint32_t data_size;
	uint32_t header_size;
	uint32_t flags;
	uint32_t version;
	uint32_t record_count;
	uint32_t reserved0;
	uint32_t reserved1;
};

struct vertex {
	float x;
	float y;
	float z;
	uint32_t color;
	float u;
	float v;
};

struct record {
	uint32_t id;
	uint32_t texture_flags;
	vertex vertices[4];
	std::array<char, 56> name;

	uint32_t texture_index() const noexcept { return texture_flags & 0xff; }
	uint32_t flags() const noexcept { return texture_flags & ~0xffu; }

	std::string_view name_view() const noexcept {
		size_t len = 0;
		while (len < name.size() && name[len] != '\0')
			++len;
		return {name.data(), len};
	}
};
#pragma pack(pop)

static_assert(sizeof(header) == 0x20);
static_assert(sizeof(vertex) == 0x18);
static_assert(sizeof(record) == 0xa0);

struct file {
	header hdr{};
	std::vector<record> records;
};

std::optional<file> parse(std::span<const uint8_t> data);
std::optional<file> parse(std::istream& is);
std::optional<file> parse(std::istream&& is);

} // namespace mpd
