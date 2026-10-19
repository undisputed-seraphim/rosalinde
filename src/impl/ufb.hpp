#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <optional>
#include <span>
#include <string>
#include <vector>

// UFB field binary (FieldBG/Field.ufb). All multi-byte fields are little endian.
//
// The file is a fixed 0x88-byte header followed by a 0x30-byte prefix, a
// 24-byte record stream, and a NUL-separated UTF-8 string pool. The 0x88-byte
// header describes the overworld field grid and the sizes of the tables that
// follow it.
//
//   0x00  char[4] "UCFB"
//   0x04  u32     version          0x05fc4d71 in Field.ufb
//   0x08  u32     header_size      16: container header only, not 0x88
//   0x0c  u32     flags            zero
//   0x10  u16     unknown10        11; not decoded
//   0x12  u16     cell_size        512 world units per field cell
//   0x14  u16     width            138 field cells
//   0x16  u16     height           105 field cells
//   0x18  i32     min_x            world-space minimum x
//   0x1c  i32     min_y            world-space minimum y
//   0x20  i32     max_x            world-space maximum x
//   0x24  i32     max_y            world-space maximum y
//   0x28  i32     size_x           max_x - min_x = (width - 1) * cell_size
//   0x2c  i32     size_y           max_y - min_y = (height - 1) * cell_size
//   0x30  u32[4]  reserved         zero
//   0x40  u32     cell_count       width * height (14,490 in Field.ufb)
//   0x44  u32     count_44         unknown table count (22,057)
//   0x48  u32     count_48         unknown table count (123,291)
//   0x4c  u32     count_4c         unknown table count (6,145)
//   0x50  u32     count_50         unknown table count (117,146)
//   0x54  u32     count_54         1,647; matches the UCB entry-index range
//   0x58  u32     count_58         unknown (11)
//   0x5c  u32     count_5c         unknown (1)
//   0x60  u32     asset_count      39 background pairs
//   0x64  u32     object_name_count 337 landmark/object names
//   0x68  u32     tile_name_count  179 overworld tile names
//   0x6c  u32     region_name_count 4 region/scenario labels
//   0x70  u16[12] unknown          12 small values; not decoded
//
// 0x88 .. 0xb7 is a 0x30-byte opaque prefix. It is copied verbatim; its first
// record-looking words overlap the terrain vertex format but are not decoded.
//
// 0xb8 .. end-of-records is a stream of 24-byte records (6,134,722 in
// Field.ufb). The leading records are terrain vertices:
//
//   u32 color / head
//   f32 u, f32 v
//   f32 x, f32 y, f32 z
//
// Later records reuse the same 24-byte stride with different meanings. Their
// partitioning is not decoded yet, so the parser exposes the whole record
// stream instead of guessing section boundaries.
//
// The string pool is located by scanning the trailing printable UTF-8 region
// and is segmented by header counts:
//
//   asset_count * 2   background path pairs: main file, then FTX texture
//   object_name_count landmark/object names (Town_*, Bridge_*, Gate_*, ...)
//   tile_name_count   overworld tile names; 162 of the 165 unique names in the
//                     NodeNetworkData corpus appear here verbatim
//   region_name_count four region labels (grassland, snowfield, desert, ...)
//
// Field.ufb contains 39 asset pairs, 337 object names, 179 tile names and 4
// region names, for 598 strings total.
//
// UCB (FieldBG/Field.ucb) is the baked spatial grid over the objects described
// here. Its object_index values are UFB object-list indices, and its
// entry_index range (0..1646) matches count_54.
namespace ufb {

#pragma pack(push, 1)
struct header {
	char magic[4];
	uint32_t version;
	uint32_t header_size;
	uint32_t flags;
	uint16_t unknown10;
	uint16_t cell_size;
	uint16_t width;
	uint16_t height;
	int32_t min_x;
	int32_t min_y;
	int32_t max_x;
	int32_t max_y;
	int32_t size_x;
	int32_t size_y;
	uint32_t reserved30;
	uint32_t reserved34;
	uint32_t reserved38;
	uint32_t reserved3c;
	uint32_t cell_count;
	uint32_t count_44;
	uint32_t count_48;
	uint32_t count_4c;
	uint32_t count_50;
	uint32_t count_54;
	uint32_t count_58;
	uint32_t count_5c;
	uint32_t asset_count;
	uint32_t object_name_count;
	uint32_t tile_name_count;
	uint32_t region_name_count;
	uint16_t unknown70[12];
};

struct record {
	uint32_t head;
	float f0;
	float f1;
	float f2;
	float f3;
	float f4;
};
#pragma pack(pop)

static_assert(sizeof(header) == 0x88);
static_assert(sizeof(record) == 24);

constexpr size_t body_offset = 0xb8;

struct asset {
	std::string main;
	std::string texture;
};

struct file {
	header hdr{};
	std::array<uint8_t, 0x30> prefix{};
	std::vector<record> records;
	std::vector<std::string> strings;
	std::vector<asset> assets;
	std::vector<std::string> object_names;
	std::vector<std::string> tile_names;
	std::vector<std::string> region_names;
	size_t strings_offset = 0;
};

std::optional<file> parse(std::span<const uint8_t> data);
std::optional<file> parse(std::istream& is);
std::optional<file> parse(std::istream&& is);

} // namespace ufb
