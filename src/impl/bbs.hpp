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
//   0x26  u16     object_count
//   0x28  u16     camera_count
//   0x2a  u16     light_count
//   0x2c  u16     fog_count
//   0x2e  u16     environment_count
//   0x30  u32     material_count
//   0x34  u32     reserved        0
//
// 0x38 .. 0x37f is a fixed 0x348-byte scene header. Its final 0x48 bytes are a
// nine-entry file-offset table: mbs_names, ftx_names, color_keys, objects,
// cameras, lights, fog, environment, materials. Earlier values are scene/camera
// setup values; they are preserved verbatim until their struct layout is
// recovered.
//
// 0x380 begins the asset-name table: mbs_count then ftx_count NUL-padded
// 32-byte names. Immediately after the names come seven fixed-record sections:
//
//   records             record size    inferred contents
//   color_keys          0x10           RGB colour keys + flags
//   objects             0x48           placed MBS objects: MBS/track ids,
//                                      position, rotation, scale, colour
//   cameras             0x6c           camera key records (layout open)
//   lights              0x44           light records (layout open)
//   fog                 0x18           optional fog/atmosphere records
//   environment         0x80           environment/weather records
//   materials           0x10           material binding records
//
// Names are structural: all 152 BattleBG BBS files parse with the same header
// and exact section sizes. objects/cameras/lights/environment/materials are
// inferred from their record shapes and per-scene counts; only `objects` has
// field offsets decoded enough to expose typed members.
//
// Verified against all 152 BattleBG BBS files: the header counts and section
// sizes consume every byte exactly. The 1,554 asset references contain 330
// unique MBS/FTX names. Only StBtShore and StBtWetland have two MBS and three
// FTX names.
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
	uint16_t object_count;
	uint16_t camera_count;
	uint16_t light_count;
	uint16_t fog_count;
	uint16_t environment_count;
	uint32_t material_count;
	uint32_t reserved5;
};

struct color_key {
	float r;
	float g;
	float b;
	uint32_t flags;
};

// One placed MBS object in the scene. The first word packs the MBS asset index
// and its variant; the second packs the track/layer ids. Position, rotation,
// scale and colour follow. The fields after `color` are not yet named.
struct object {
	uint16_t mbs_index;
	uint16_t mbs_variant;
	uint16_t track_index;
	uint16_t layer_index;
	float x;
	float y;
	float z;
	float rotation_x;
	float rotation_y;
	float rotation_z;
	float scale_x;
	float scale_y;
	int32_t color_key;
	uint32_t color;
	uint32_t unknown_30;
	uint32_t unknown_34;
	float alpha;
	uint32_t unknown_3c;
	float unknown_40;
	uint32_t flags;
};

struct camera_record {
	std::array<uint8_t, 108> data;
};

struct light_record {
	std::array<uint8_t, 68> data;
};

struct fog_record {
	std::array<uint8_t, 24> data;
};

struct environment_record {
	std::array<uint8_t, 128> data;
};

struct material_record {
	std::array<uint8_t, 16> data;
};
#pragma pack(pop)

static_assert(sizeof(header) == 0x38);
static_assert(sizeof(color_key) == 0x10);
static_assert(sizeof(object) == 0x48);
static_assert(sizeof(camera_record) == 0x6c);
static_assert(sizeof(light_record) == 0x44);
static_assert(sizeof(fog_record) == 0x18);
static_assert(sizeof(environment_record) == 0x80);
static_assert(sizeof(material_record) == 0x10);

constexpr size_t names_offset = 0x380;
constexpr size_t prefix_offset = 0x38;

// The prefix offset table occupies the nine u64 words immediately before the
// names. Values are absolute file offsets.
struct section_offsets {
	uint64_t mbs_names = 0;
	uint64_t ftx_names = 0;
	uint64_t color_keys = 0;
	uint64_t objects = 0;
	uint64_t cameras = 0;
	uint64_t lights = 0;
	uint64_t fog = 0;
	uint64_t environment = 0;
	uint64_t materials = 0;
};
static_assert(sizeof(section_offsets) == 0x48);

struct file {
	header hdr{};
	std::array<uint8_t, names_offset - prefix_offset> prefix{};
	section_offsets offsets{};
	std::vector<std::string> mbs_names;
	std::vector<std::string> ftx_names;
	std::vector<color_key> color_keys;
	std::vector<object> objects;
	std::vector<camera_record> cameras;
	std::vector<light_record> lights;
	std::vector<fog_record> fog;
	std::vector<environment_record> environment;
	std::vector<material_record> materials;
};

std::optional<file> parse(std::span<const uint8_t> data);
std::optional<file> parse(std::istream& is);
std::optional<file> parse(std::istream&& is);

} // namespace bbs
