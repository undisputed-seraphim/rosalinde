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
	uint32_t s4_source_tile_id; // source tile/UV id; usually 1:1 with s1_uv_id
	uint8_t color_id;
	uint8_t flags;
	uint8_t blend_id;
	uint8_t tex_id;
	uint32_t attributes;
	uint16_t s1_uv_id; // id of texture uv
	uint16_t s0_color_row_id; // id of fog color
	uint16_t s2_vertex_id; // id of vertex
	uint16_t _pad;
};

struct section_5 {
	uint16_t s3_id;
	uint8_t s5_hitbox_kind; // hitbox kind/group; gameplay use not mapped
	uint8_t s5_hitbox_control; // bit0: no geometry; bit1: disable interpolation
	uint32_t flags; // hitbox flags; exact bits not mapped
};

// Keyframe
struct section_6 {
	float left;
	float top;
	float right;
	float bottom;
	uint32_t s4_layer_start; // starting layer id
	uint16_t s5_hitbox_start; // starting hitbox id
	uint16_t s4_layer_count; // nr of layers
	uint8_t s5_hitbox_count;	// nr of hitboxes
	uint8_t flags; // keyframe flags; exact bits not mapped
	uint16_t _pad0;
};

struct section_7 {
	glm::vec3 move;
	glm::vec3 rotate;
	glm::vec2 scale;
	uint32_t fog;
};

struct section_8 {
	uint16_t s6_keyframe_id;
	uint16_t _pad0;
	uint16_t s7_transform_id;
	uint16_t s8_duration_ticks;
	uint32_t flags;
	uint16_t s8_loop_target; // absolute target offset; engine treats negative as 0
	uint8_t s5s3_hitbox_interpolation;
	uint8_t interpolation_rate;
	uint8_t s7_transform_interpolation;
	uint8_t s6_keyframe_interpolation;
	uint8_t s8_render_value; // render-object +0x1e4; exact visual meaning open
	uint8_t s8_draw_offset; // signed draw/sort offset, not a turn count
	uint32_t s8_event_arg; // 0x80 callback payload word 0; nonzero in 43 frames
	uint16_t s8_event_flag; // 0x80 callback flag index; nonzero in 3562 frames
	uint16_t s8_event_value; // 0x80 callback payload; nonzero in 8 frames
	uint32_t s8_reserved; // zero in all 969315 corpus frames
};

struct section_9 {
	float left;
	float top;
	float right;
	float bottom;
	char name[24];
	uint16_t sa_sequence_start; // starting index into section_a
	uint8_t sa_sequence_count;
	uint8_t sa_main_sequence;
	uint16_t sa_sb_start;
	uint8_t sa_sb_count;
	uint8_t disabled; // 0 = enabled, 1 = disabled
};

struct section_a {
	uint16_t s8_frame_start;		  // starting index into section_8
	uint16_t s8_frame_count;		  // number of section_8 entries
	uint32_t s8_total_ticks;	  // total tick sum of all frame durations
	int32_t s8_intro_ticks;  // intro portion (ticks played once before loop). -1 = no intro
	int32_t commit_ticks; // uninterruptible window (ticks). -1 = no commit window
	uint16_t sb_id;
	uint8_t sb_no; // always 1 when sb_id > 0, else 0
	uint8_t s8_start_offset; // skip first frame: 0 = play [s8_frame_start ..], 1 = play [s8_frame_start+1 ..]
	uint16_t sa_track_slot;
	uint16_t _pad; // always 0
};

struct section_b {
	uint32_t speed_num; // per-frame speed numerator
	uint16_t speed_den; // per-frame speed denominator
	uint16_t _pad0;		// always 0
	uint32_t timing_a;	// 0 on characters; 40-140 on BGBtRoad_h
	uint16_t oneshot;	// 0-3 on characters; 10 on BGBtRoad_h
	uint16_t timing_b;	// 0 on characters; 10-100 on BGBtRoad_h
	uint32_t _pad1;		// always 0
};
static_assert(sizeof(section_b) == 0x14);
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
		ENABLED = 0x01,
		SKIP = 0x02,
		NOTEX = 0x04,
	};

	enum s8flag : uint32_t {
		// clang-format off
		FLIPX    = 0x01,   // horizontal mirror
		FLIPY    = 0x02,   // vertical mirror (rare)
		JUMP     = 0x04,   // next frame offset = s8_loop_target (sequence-relative absolute)
		HOLD     = 0x08,   // no frame advance; holds the current frame
		ACTIVE   = 0x20,   // frame is renderable
		HIDDEN   = 0x40,   // skip update and draw (s8_frame_is_hidden / sprite_ease_and_build)
		EVENT    = 0x80,   // invokes the TrackInstance +0xa0 payload callback
		NODRAW   = 0x400,  // update continues, but sprite_build_matrix creates no render object
		LAST     = 0x800,  // end-of-sequence marker (loop point when also JUMP)
		STOP     = 0x1000, // same no-advance path as LAST; one corpus occurrence
		GLOBAL   = 0x2000, // sets global renderer flags; exact pass not mapped
		// 0x10, 0x100, 0x200 never occurred in 722 files
		// clang-format on
	};
};

} // namespace mbs
