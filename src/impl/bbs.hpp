#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <optional>
#include <span>
#include <string>
#include <vector>

// BBS battle-background scene setup (BattleBG/*.bbs). All multi-byte fields are
// little endian.
//
// BBS is a battle/event background scene descriptor: it lists the MBS/FTX
// layers used by the scene and carries several fixed-size scene-record tables.
// The referenced MBS/FTX names are relative to the game asset search path, not
// always to BattleBG (for example FieldEffect.mbs resolves under Effect/ and
// Class_Change_BG under GUI/).
//
//   0x00  char[4] "UBBS"
//   0x04  u32     reserved        0
//   0x08  u32     header_size     0x10 (container header only)
//   0x0c  u32     reserved        0
//   0x10  u32     version         3
//   0x14  u32     reserved        0
//   0x18  u32     reserved        0
//   0x1c  u32     reserved        0
//   0x20  u16     mbs_count
//   0x22  u16     ftx_count       equal to mbs_count except StBtShore/StBtWetland
//   0x24  u16     color_key_count
//   0x26  u16     section_72_count
//   0x28  u16     section_108_count
//   0x2a  u16     section_68_count
//   0x2c  u16     section_24_count
//   0x2e  u16     section_128_count
//   0x30  u32     section_16_count
//   0x34  u32     reserved        0
//
// 0x38 .. 0x37f is a fixed 0x348-byte header prefix. It contains scene values
// whose layout is not decoded; the parser preserves it verbatim.
//
// 0x380 begins the asset-name table: mbs_count then ftx_count NUL-padded
// 32-byte names. Immediately after the names come seven fixed-record sections:
//
//   section             record size    record contents
//   color_key_count     0x10           3 floats + u32 (likely colour keys)
//   section_72_count    0x48           opaque in this parser
//   section_108_count   0x6c           opaque in this parser
//   section_68_count    0x44           opaque in this parser
//   section_24_count    0x18           opaque in this parser
//   section_128_count   0x80           opaque in this parser
//   section_16_count    0x10           opaque in this parser
//
// Verified against all 152 BattleBG BBS files: the header counts and section
// sizes consume every byte exactly. The 1,554 asset references contain 330
// unique MBS/FTX names. Only StBtShore and StBtWetland have two MBS and three
// FTX names. The section contents are deliberately left raw until interpreted.
namespace bbs {

#pragma pack(push, 1)
struct header {
	char magic[4];
	uint32_t reserved0;
	uint32_t header_size;
	uint32_t reserved1;
	uint32_t version;
	uint32_t reserved2;
	uint32_t reserved3;
	uint32_t reserved4;
	uint16_t mbs_count;
	uint16_t ftx_count;
	uint16_t color_key_count;
	uint16_t section_72_count;
	uint16_t section_108_count;
	uint16_t section_68_count;
	uint16_t section_24_count;
	uint16_t section_128_count;
	uint32_t section_16_count;
	uint32_t reserved5;
};

struct color_key {
	float r;
	float g;
	float b;
	uint32_t flags;
};

struct record16 {
	std::array<uint8_t, 16> data;
};

struct record24 {
	std::array<uint8_t, 24> data;
};

struct record68 {
	std::array<uint8_t, 68> data;
};

struct record72 {
	std::array<uint8_t, 72> data;
};

struct record108 {
	std::array<uint8_t, 108> data;
};

struct record128 {
	std::array<uint8_t, 128> data;
};
#pragma pack(pop)

static_assert(sizeof(header) == 0x38);
static_assert(sizeof(color_key) == 0x10);
static_assert(sizeof(record16) == 0x10);
static_assert(sizeof(record24) == 0x18);
static_assert(sizeof(record68) == 0x44);
static_assert(sizeof(record72) == 0x48);
static_assert(sizeof(record108) == 0x6c);
static_assert(sizeof(record128) == 0x80);

constexpr size_t names_offset = 0x380;
constexpr size_t prefix_offset = 0x38;

struct file {
	header hdr{};
	std::array<uint8_t, names_offset - prefix_offset> prefix{};
	std::vector<std::string> mbs_names;
	std::vector<std::string> ftx_names;
	std::vector<color_key> color_keys;
	std::vector<record72> section_72;
	std::vector<record108> section_108;
	std::vector<record68> section_68;
	std::vector<record24> section_24;
	std::vector<record128> section_128;
	std::vector<record16> section_16;
};

std::optional<file> parse(std::span<const uint8_t> data);
std::optional<file> parse(std::istream& is);
std::optional<file> parse(std::istream&& is);

} // namespace bbs
