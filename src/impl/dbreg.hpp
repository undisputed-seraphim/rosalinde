#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// DbReg tuning database (DbReg/*.drg). All multi-byte fields are little endian.
//
// The directory contains two related files:
//
//   DbRegDb.drg  catalog: header, source-file records and key/description records
//   DbReg.drg    values:  one 32-bit value per catalog key
//
// DbRegDb.drg header (0x38 bytes):
//
//   0x00  char[4] "DRG\0"
//   0x04  u32     source_count       96
//   0x08  u32     key_count          6559
//   0x0c  u32     unknown
//   0x10  u32     unknown            40
//   0x14  u32     unknown
//   0x18  u32     key_offset         0x3628
//   0x1c  u32     unknown
//   0x20  u32     unknown
//   0x24  u32     unknown
//   0x28  u32     unknown            63
//   0x2c  u32     unknown
//   0x30  u32     unknown
//   0x34  u32     unknown
//
// Source records start at 0x38, 0x90 bytes each:
//
//   0x00  char[64] source path      e.g. C:/Project2/Unicorn/Soft/DbReg/Debug.txt
//   0x40  char[64] source tag       e.g. DEBUG
//   0x80  u32[4]   unknown          (13, 63, 1, 0) for the first record
//
// Key records start at key_offset, 0xD0 bytes each, key_count total:
//
//   0x00  u32     flags
//   0x04  f32     default_value
//   0x08  u32     unknown
//   0x0c  u32     unknown
//   0x10  char[64] key
//   0x50  char[128] localized description
//
// DbReg.drg is a 4-byte "DRG\0" magic followed by key_count 32-bit values. In
// the shipped pair, all 6,559 raw values match the corresponding catalog
// default_value bit-for-bit. The remaining flag/value fields are not decoded
// yet.
//
// Verified against the two files in DbReg: the catalog consumes every byte with
// 96 source records and 6,559 key records, and DbReg.drg is exactly 4 +
// 6,559 * 4 bytes.
namespace dbreg {

enum class kind {
	catalog,
	values,
};

#pragma pack(push, 1)
struct header {
	char magic[4];
	uint32_t source_count;
	uint32_t key_count;
	uint32_t unknown0c;
	uint32_t unknown10;
	uint32_t unknown14;
	uint32_t key_offset;
	uint32_t unknown1c;
	uint32_t unknown20;
	uint32_t unknown24;
	uint32_t unknown28;
	uint32_t unknown2c;
	uint32_t unknown30;
	uint32_t unknown34;
};

struct source_entry {
	char path[64];
	char tag[64];
	uint32_t unknown[4];

	std::string_view path_view() const;
	std::string_view tag_view() const;
};

struct key_entry {
	uint32_t flags;
	float default_value;
	uint32_t unknown0;
	uint32_t unknown1;
	char key[64];
	char description[128];

	std::string_view key_view() const;
	std::string_view description_view() const;
};
#pragma pack(pop)

static_assert(sizeof(header) == 0x38);
static_assert(sizeof(source_entry) == 0x90);
static_assert(sizeof(key_entry) == 0xd0);

struct file {
	kind type = kind::values;
	header hdr{};
	std::vector<source_entry> sources;
	std::vector<key_entry> keys;
	std::vector<uint32_t> values;
};

std::optional<file> parse(std::span<const uint8_t> data);
std::optional<file> parse(std::istream& is);
std::optional<file> parse(std::istream&& is);

} // namespace dbreg
