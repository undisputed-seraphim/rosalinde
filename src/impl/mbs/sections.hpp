#pragma once

#include <cstdint>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <iosfwd>
#include <vector>

namespace mbs {

#pragma pack(push, 1)

static_assert(sizeof(glm::vec2) == sizeof(float) * 2);
static_assert(sizeof(glm::vec3) == sizeof(float) * 3);

struct file_header {
	char magic[4];
	uint32_t file_size;
	uint32_t header_size;
	uint32_t _pad0;
	uint16_t _unk0;
	uint16_t _unk1;
	uint16_t version;
	uint16_t _unk2;
	uint32_t _unk3;
	uint32_t _pad1;
};

// Fragment RGBA
struct section_0 {
	uint32_t center;
	uint32_t colors[4]; // 4xRGBA
	uint32_t topleft;
};

// Texture UV coords
struct section_1 {
	float center[2];
	glm::vec2 values[4];
	float topleft[2];
};

// Vertex
struct section_2 {
	float unused0[2];
	glm::vec2 values[4];
	float topleft[2];
};

// Hitbox
struct section_3 {
	glm::vec2 hitbox[4];
	glm::vec3 normals[4]; // unit vectors: X+, Y-, X-, Y+
};

// Keyframe Layer
struct section_4 {
	uint32_t _unk0;
	uint8_t color_id;
	uint8_t flags;
	uint8_t blend_id;
	uint8_t tex_id;
	uint32_t attributes;
	uint16_t s1_id; // id of texture uv
	uint16_t s0_id; // id of fog color
	uint16_t s2_id; // id of vertex
	uint16_t _pad;
};

struct section_5 {
	uint16_t s3_id;
	uint8_t _unk0; // all 0
	uint8_t _unk1; // all 0
	uint32_t flags;
};

// Keyframe
struct section_6 {
	float left;
	float top;
	float right;
	float bottom;
	uint32_t s4_id; // starting layer id
	uint16_t s5_id; // starting hitbox id
	uint16_t s4_no; // nr of layers
	uint8_t s5_no;	// nr of hitboxes
	uint8_t flags;
	uint16_t _pad0;
};

struct section_7 {
	glm::vec3 move;
	glm::vec3 rotate;
	glm::vec2 scale;
	uint32_t fog;
};

struct section_8 {
	uint16_t s6_id;
	uint16_t _pad0;
	uint16_t s7_id;
	uint16_t frames;
	uint32_t flags;
	uint16_t loop_s8_id;
	uint8_t s5s3_interpolation;
	uint8_t interpolation_rate;
	uint8_t s7_interpolation;
	uint8_t s6_interpolation;
	uint8_t s0s1s2_interpolation;
	uint8_t n_180;
	uint32_t _pad1;
	uint16_t _pad2;
	uint16_t sfx_mute;
	uint32_t sfx_id;
};

struct section_9 {
	float left;
	float top;
	float right;
	float bottom;
	char name[24];
	uint16_t sa_set_id; // starting index into section_a
	uint8_t sa_set_no;
	uint8_t sa_set_main;
	uint16_t sa_sb_set_id;
	uint8_t sa_sb_set_no;
	uint8_t disabled; // 0 = enabled, 1 = disabled
};

struct section_a {
	uint16_t s8_id;		  // starting index into section_8
	uint16_t s8_no;		  // number of section_8 entries
	uint32_t s8_sum;	  // total tick sum of all frame durations
	int32_t s8_sum_once;  // intro portion (ticks played once before loop). -1 = no intro
	int32_t commit_ticks; // uninterruptible window (ticks). -1 = no commit window
	uint16_t sb_id;
	uint8_t sb_no; // always 1 when sb_id > 0, else 0
	uint8_t s8_st; // skip first frame: 0 = play [s8_id ..], 1 = play [s8_id+1 ..]
	uint16_t track_id;
	uint16_t _pad; // always 0
};

struct section_b {
	uint32_t speed_num; // per-frame speed numerator (3–75)
	uint16_t speed_den; // per-frame speed denominator (1–17)
	uint32_t _pad0;     // always 0
	uint16_t _pad1;     // always 0
	uint16_t oneshot;   // one-shot flag (0–3)
	uint32_t _pad2;     // always 0
	uint16_t _pad3;     // always 0
};
#pragma pack(pop)

struct v77 {
	std::vector<section_0> s0;
	std::vector<section_1> s1;
	std::vector<section_2> s2;
	std::vector<section_3> s3;
	std::vector<section_4> s4;
	std::vector<section_5> s5;
	std::vector<section_6> s6;
	std::vector<section_7> s7;
	std::vector<section_8> s8;
	std::vector<section_9> s9;
	std::vector<section_a> sa;
	std::vector<section_b> sb;

	friend std::istream& operator>>(std::istream&, v77&);

	enum s4flag : uint8_t {
		SKIP = 0x02,
		NOTEX = 0x04,
	};

	enum s8flag : uint32_t {
		// clang-format off
		FLIPX  = 0x01,   // horizontal mirror
		FLIPY  = 0x02,   // vertical mirror (rare)
		JUMP   = 0x04,   // control-flow: next = current + loop_s8_id
		ACTIVE = 0x20,   // frame is renderable (cleared on hitbox-only / transition frames)
		POSE   = 0x80,   // non-idle body pose (GUARD, KNOCKBACK, DOWN, SIT)
		HITBOX = 0x400,  // hitbox-only frame (no sprite layers)
		LAST   = 0x800,  // end-of-sequence marker
		//?    = 0x2000, // rarely set, purpose unclear (may be "no blend" / "hold")
		// clang-format on
	};
};

} // namespace mbs
