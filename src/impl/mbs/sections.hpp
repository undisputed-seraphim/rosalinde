#pragma once

#include "mbs.hpp"

#include <cstdint>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <iosfwd>
#include <vector>

namespace mbs {

#pragma pack(push, 1)

static_assert(sizeof(glm::vec2) == sizeof(float) * 2);
static_assert(sizeof(glm::vec3) == sizeof(float) * 3);

// Packed FMBS header. `magic` is "FMBS"; `version` is 0x76 or 0x77.
struct file_header {
	char magic[4];		  // "FMBS"
	uint32_t file_size;	  // total file size in bytes
	uint32_t header_size; // size of this header plus the descriptor table
	uint32_t _pad0;		  // reserved; observed zero
	uint16_t _unk0;		  // OPEN: stable corpus value, no known consumer
	uint16_t _unk1;		  // OPEN: stable corpus value, no known consumer
	uint16_t version;	  // 0x76 or 0x77
	uint16_t _unk2;		  // OPEN: file-dependent value, not a section/texture/track count
	uint32_t _unk3;		  // OPEN: stable corpus value, no known consumer
	uint32_t _pad1;		  // reserved; observed zero
};

// Per-corner RGBA colours. `center` and `topleft` are packed RGBA values; the
// four `colors` entries are the quad corners. Character tables are small colour
// ramps; background tables can provide full atmospheric gradients.
struct section_0 {
	uint32_t center;	// packed centre RGBA
	uint32_t colors[4]; // 4xRGBA
	uint32_t topleft;	// packed top-left RGBA
};

// Per-quad UV coordinates in texture-texel space. `center`/`topleft` are the
// reference points used with the four corner values.
struct section_1 {
	float center[2];	 // centre UV
	glm::vec2 values[4]; // per-corner UVs
	float topleft[2];	 // top-left UV
};

// Per-quad corner positions. These vertices are the actual rendered geometry and
// the source for content bounds; keyframe bounds are only hints.
struct section_2 {
	float unused0[2];	 // OPEN: no known consumer
	glm::vec2 values[4]; // per-corner vertex positions
	float topleft[2];	 // top-left vertex reference
};

// Hitbox geometry: four quad points and their unit normals. Order is
// X+, Y-, X-, Y+.
struct section_3 {
	glm::vec2 hitbox[4];  // quad corners
	glm::vec3 normals[4]; // unit normals: X+, Y-, X-, Y+
};

// One drawable layer. The four IDs select the UV, vertex, colour, and texture
// data for this layer. `attributes` is a suppression mask, not an allowlist:
// the renderer skips the layer when `(attributes & appearance_mask) != 0` and
// draws it when the result is zero. `appearance_mask` is the engine's
// per-character `chara_model` mask (ORed with 0xfe000000); a zero mask draws
// every non-SKIP layer.
struct section_4 {
	uint32_t s4_source_tile_id; // source tile/UV identifier; runtime effect OPEN
	uint8_t color_id;			// 1..3 selects a CharaSet colour-preset role; 0 = section_0
	uint8_t flags;				// layer flags; see s4flag
	uint8_t blend_id;			// blend mode; engine enum OPEN, viewer uses 0/1/2/3
	uint8_t tex_id;				// index into the FTX texture array
	uint32_t attributes;		// suppression bits; draw when (attributes & appearance_mask) == 0
	uint16_t s1_uv_id;			// index into section_1
	uint16_t s0_color_row_id;	// default per-corner colours; colour_id overrides when enabled
	uint16_t s2_vertex_id;		// index into section_2
	uint16_t _pad;				// reserved; observed zero
};

// One hitbox entry. `s3_id` points at the hitbox geometry in section_3.
struct section_5 {
	uint16_t s3_id;			   // index into section_3
	uint8_t s5_hitbox_kind;	   // gameplay kind/group; mapping OPEN
	uint8_t s5_hitbox_control; // bit0 = no geometry; bit1 = disable interpolation
	uint32_t flags;			   // hitbox flags; bit meanings OPEN
};

// One keyframe: a bounding box plus contiguous section_4/section_5 ranges.
// Keyframe bounds are not reliable content bounds; use section_2 vertices for
// geometry.
struct section_6 {
	float left;				  // keyframe bounds, not content bounds
	float top;				  // keyframe bounds, not content bounds
	float right;			  // keyframe bounds, not content bounds
	float bottom;			  // keyframe bounds, not content bounds
	uint32_t s4_layer_start;  // starting layer index in section_4
	uint16_t s5_hitbox_start; // starting hitbox index in section_5
	uint16_t s4_layer_count;  // number of layer rows
	uint8_t s5_hitbox_count;  // number of hitbox rows
	uint8_t flags;			  // per-keyframe mask; semantics OPEN
	uint16_t _pad0;			  // reserved; observed zero
};

// Per-frame transform. Runtime composition is scale -> translate -> rotate;
// see `s7_matrix` in v77.cpp.
struct section_7 {
	glm::vec3 move;	  // translation
	glm::vec3 rotate; // Euler rotation
	glm::vec2 scale;  // scale
	uint32_t fog;	  // packed ARGB colour tint; alpha is opacity/strength, not depth
};

// One animation frame. Frames are indexed by sequences in section_a.
struct section_8 {
	uint16_t s6_keyframe_id;			// index into section_6
	uint16_t _pad0;						// reserved; observed zero
	uint16_t s7_transform_id;			// index into section_7
	uint16_t s8_duration_ticks;			// duration in animation ticks
	uint32_t flags;						// frame flags; see s8flag
	uint16_t s8_loop_target;			// absolute sequence-relative jump target; 0 restarts at the first played frame
	uint8_t s5s3_hitbox_interpolation;	// 0=linear, 1=light, 2=cubic
	uint8_t interpolation_rate;			// interpolation frequency/rate
	uint8_t s7_transform_interpolation; // 0/1=linear, 2=cubic
	uint8_t s6_keyframe_interpolation;	// 0/1=linear, 2=cubic
	uint8_t s8_render_value;			// render-object +0x1e4; visual meaning OPEN
	uint8_t s8_draw_offset;				// signed draw/sort offset, not a turn count
	uint32_t s8_event_arg;				// 0x80 callback payload word 0
	uint16_t s8_event_flag;				// 0x80 callback renderer-flag index
	uint16_t s8_event_value;			// 0x80 callback payload; meaning OPEN
	uint32_t s8_reserved;				// observed zero; keep reserved
};

// One track: a named animation for a character, or an element/state for a
// background. Track liveness and selection are external game logic.
struct section_9 {
	float left;					// framing hint, not sprite bounds
	float top;					// framing hint, not sprite bounds
	float right;				// framing hint, not sprite bounds
	float bottom;				// framing hint, not sprite bounds
	char name[24];				// asset-defined track identifier
	uint16_t sa_sequence_start; // starting index into section_a
	uint8_t sa_sequence_count;	// number of sequence rows
	uint8_t sa_main_sequence;	// 0-based main/default sequence index
	uint16_t sa_sb_start;		// starting index into section_b
	uint8_t sa_sb_count;		// number of section_b rows
	uint8_t disabled;			// 1 = reserved/padding track
};

// One sequence. Multiple sequences in a track are ordered timeline segments;
// their frame ranges normally tile contiguously.
struct section_a {
	uint16_t s8_frame_start; // starting index into section_8
	uint16_t s8_frame_count; // number of frame rows
	uint32_t s8_total_ticks; // total tick sum of the sequence
	int32_t s8_intro_ticks;	 // intro ticks played once before the loop; -1 = single pass
	int32_t commit_ticks;	 // uninterruptible window in ticks; -1 = none
	uint16_t sb_id;			 // section_b row id; 0 = none
	uint8_t sb_no;			 // 1 when sb_id is set, otherwise 0
	uint8_t s8_start_offset; // character: skip-first-frame; background: sub-sprite index
	uint16_t sa_track_slot;	 // sequence slot within the parent track; gaps are reserved
	uint16_t _pad;			 // reserved; observed zero
};

// Shared transition/background-timing row. In character transition modes 1-3,
// the continuation is the next enabled section_9 track. Mode 10 is background
// timing and must not be treated as a track transition.
struct section_b {
	uint32_t rate_num;			  // per-frame rate numerator; consumer OPEN
	uint16_t rate_den;			  // per-frame rate denominator; consumer OPEN
	uint16_t _pad0;				  // reserved; observed zero
	uint32_t background_timing_a; // background timing field; semantics OPEN
	uint16_t transition_mode;	  // 0 = none, 1-3 = character continuation, 10 = background timing
	uint16_t background_timing_b; // background timing field; semantics OPEN
	uint32_t _pad1;				  // reserved; observed zero
};
static_assert(sizeof(section_b) == 0x14);
#pragma pack(pop)

// Decoded V77 sections. Values in one section refer to other sections by index;
// see the model in mbs.hpp.
struct v77 {
	std::vector<section_0> s0; // colours
	std::vector<section_1> s1; // UVs
	std::vector<section_2> s2; // vertices
	std::vector<section_3> s3; // hitbox geometry
	std::vector<section_4> s4; // keyframe layers
	std::vector<section_5> s5; // hitbox entries
	std::vector<section_6> s6; // keyframes
	std::vector<section_7> s7; // transforms
	std::vector<section_8> s8; // animation frames
	std::vector<section_9> s9; // tracks
	std::vector<section_a> sa; // sequences
	std::vector<section_b> sb; // transition/background timing

	friend std::istream& operator>>(std::istream&, v77&);

	// Layer flags. 0x08 and 0x10 occur in assets but their renderer semantics
	// are OPEN; do not infer them from the known bits.
	enum s4flag : uint8_t {
		ENABLED = 0x01, // valid layer
		SKIP = 0x02,	// set = layer is skipped by the renderer
		NOTEX = 0x04,	// no texture; solid colour/veil
	};

	// Frame flags. ACTIVE, HIDDEN, and NODRAW are independent gates; HIDDEN is a
	// hard update/draw suppress, while NODRAW only suppresses the generic render
	// object. JUMP targets are absolute sequence-relative offsets.
	enum s8flag : uint32_t {
		// clang-format off
		FLIPX    = 0x01,   // horizontal mirror
		FLIPY    = 0x02,   // vertical mirror
		JUMP     = 0x04,   // next offset = s8_loop_target (absolute)
		HOLD     = 0x08,   // no frame advance; hold current frame
		ACTIVE   = 0x20,   // frame is renderable
		HIDDEN   = 0x40,   // skip update and draw
		EVENT    = 0x80,   // invokes the TrackInstance +0xa0 payload callback
		NODRAW   = 0x400,  // update continues, but no generic render object
		LAST     = 0x800,  // end-of-sequence marker
		STOP     = 0x1000, // same no-advance path as LAST; purpose OPEN
		GLOBAL   = 0x2000, // global/track renderer flags; exact pass OPEN
		// 0x10, 0x100, 0x200: no known meaning
		// clang-format on
	};
};

} // namespace mbs
