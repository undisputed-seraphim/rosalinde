#pragma once

#include "../mbs.hpp"
#include <cstdint>
#include <iosfwd>
#include <vector>

namespace MBS_ {

#pragma pack(push, 1)
// Fragment RGBA - nds_quad18c
struct section_0 {
	uint32_t center;
	uint32_t colors[4]; // 4xRGBA
	uint32_t topleft;
};

// Texture UV coords - nds_quad30p
struct section_1 {
	float center[2];
	float values[8]; // 4x2
	float topleft[2];
};

// Vertex - nds_quad30p
struct section_2 {
	float unused0[2];
	float values[8]; // 4x2
	float topleft[2];
};

// Hitbox?
struct section_3 {
	float hitbox[8]; // 4x2
	float _xyz[12];	 // 4x3 [1, -0, 0 , -0, -1, 0 , -1, -0, 0 , -0, 1, 0]
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
	uint8_t _unk0;
	uint8_t _unk1;
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

// We want to turn this struct into a translate-rotate-scale 4x4 matrix
struct section_7 {
	float move[3];
	float rotate[3];
	float scale[2];
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

// Animation names
struct section_9 {
	float left;
	float top;
	float right;
	float bottom;
	char name[24];
	uint16_t sa_set_id;
	uint8_t sa_set_no;
	uint8_t sa_set_main;
	uint16_t sa_sb_set_id;
	uint8_t sa_sb_set_no;
	uint8_t _0_1;
};

struct section_a {
	uint16_t s8_id;
	uint16_t s8_no;
	uint32_t s8_sum;
	uint32_t s8_sum_once;
	uint32_t _unk0;
	uint16_t sb_id;
	uint8_t sb_no;
	uint8_t s8_st; // 0 1 bool
	uint16_t _unk1;
	uint16_t _unk2;
};

struct section_b {
	uint32_t _unk0;
	uint16_t _unk1;
	uint32_t _unk2;
	uint16_t _unk3;
	uint16_t _unk4;
	uint32_t _unk5;
	uint16_t _pad;
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

	static v77 read(std::istream&);
	friend std::ostream& operator<<(std::ostream&, const v77&);

	Quad to_quad() const;
};

} // namespace MBS_
