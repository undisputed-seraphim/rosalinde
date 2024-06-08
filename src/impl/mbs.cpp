#include "mbs.hpp"
#include "utils.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

struct lookup_entry {
	int16_t c;			// offset location offset
	int16_t c_size;		// offset value size
	int16_t p;			// offset location offset 2
	int16_t row_length; // row length
	uint32_t h = 0;		// Num entries
	uint32_t offset = 0;
	uint32_t data_size = 0;
};

constexpr auto table_v77 = std::array{
	lookup_entry{0x54, 4, 0xb0, 0x18},	// 1:s0
	lookup_entry{0x58, 4, 0xb8, 0x30},	// 2:s1
	lookup_entry{0x5c, 4, 0xc0, 0x30},	// 3:s2
	lookup_entry{0x60, 4, 0xc8, 0x50},	// 4:s3 bg=0
	lookup_entry{0x50, 4, 0xd0, 0x14},	// 0:s4
	lookup_entry{0x64, 2, 0xd8, 0x8},	// 5:s5 bg=0
	lookup_entry{0x6a, 2, 0xe0, 0x1c},	// 8:s6
	lookup_entry{0x66, 2, 0xe8, 0x24},	// 6:s7
	lookup_entry{0x68, 2, 0xf0, 0x20},	// 7:s8
	lookup_entry{0x6c, 2, 0xf8, 0x30},	// 9:s9
	lookup_entry{0x6e, 2, 0x100, 0x18}, // a:sa
	lookup_entry{0x72, 2, 0x108, 0x14}, // b:sb
};

// Faulty struct, do not use
struct v77_1 {
	uint32_t longvals[5];
	uint16_t shortvals[7];
	uint64_t _unknown1[2];
	char name[32];
	uint64_t _unknown2[2];
	uint64_t offsets[12];
};

#pragma pack(push, 1)
struct mbs_header {
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

	static constexpr std::string_view FMBS = "FMBS";
};

struct section_0 {
	// nds_quad18c
	int32_t center;
	int32_t c0;
	int32_t c1;
	int32_t c2;
	int32_t c3;
	int32_t c0_2;
};

struct section_1 {
	// nds_quad30p
	float values[12];
};

struct section_2 {
	// nds_quad30p
	float values[12];
};

struct section_3 {
	float rect_x1;
	float rect_y1;
	float rect_x2;
	float rect_y2;
	float rect_x3;
	float rect_y3;
	float rect_x4;
	float rect_y4;
	float nx1;
	float ny1;
	float nz1; // = 0
	float nx2;
	float ny2;
	float nz2; // = 0
	float nx3;
	float ny3;
	float nz3; // = 0
	float nx4;
	float ny4;
	float nz4; // = 0
};

struct section_4 {
	uint32_t _unk0;
	uint8_t color_id;
	uint8_t flags;
	uint8_t blend_id;
	uint8_t tex_id;
	uint32_t attributes;
	uint16_t s1_id;
	uint16_t s0_id;
	uint16_t s2_id;
	uint16_t _pad;
};

struct section_5 {
	uint16_t s3_id;
	uint8_t _unk0;
	uint8_t _unk1;
	uint32_t flags;
};

struct section_6 {
	float left;
	float top;
	float right;
	float bottom;
	uint32_t s4_id;
	uint16_t s5_id;
	uint16_t s4_no;
	uint8_t s5_no;
	uint8_t flags;
	uint16_t _pad0;
};

struct section_7 {
	float move[3];
	float rotate[3];
	float scale[2];
	int32_t _pad0;
};

struct section_8 {
	uint16_t s6_id;
	uint16_t _pad0;
	uint16_t s7_id;
	uint16_t frames;
	uint32_t flags;
	uint16_t s8_id;
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
	uint32_t s8_num;
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

	friend std::ostream& operator<<(std::ostream&, const v77&);
};

template <typename T>
std::vector<T> read_block(const lookup_entry& e, std::istream& is) {
	if (e.c == 0 || e.p == 0) {
		return {};
	}
	std::vector<T> entries;
	entries.reserve(e.h);
	is.seekg(e.offset, std::ios::beg);
	for (uint32_t i = 0; i < e.h; ++i) {
		entries.push_back(read_value<T>(is));
	}
	return entries;
}

v77 handle_mbsv77(std::istream& is) {
	auto table = table_v77; // Make a copy
	for (auto& [c, cs, p, rl, h, o, ds] : table) {
		if (c == 0 || p == 0) {
			continue;
		}
		is.seekg(c, std::ios::beg);
		switch (cs) {
		case 2: {
			h = read_value<uint16_t>(is);
			break;
		}
		case 4: {
			h = read_value<uint32_t>(is);
			break;
		}
		}
		is.seekg(p, std::ios::beg);
		o = read_value<uint32_t>(is);
		ds = h * rl;
	}
	// Do NOT sort or change the order of table entries!

	return v77{
		read_block<section_0>(table[0x0], is),
		read_block<section_1>(table[0x1], is),
		read_block<section_2>(table[0x2], is),
		read_block<section_3>(table[0x3], is),
		read_block<section_4>(table[0x4], is),
		read_block<section_5>(table[0x5], is),
		read_block<section_6>(table[0x6], is),
		read_block<section_7>(table[0x7], is),
		read_block<section_8>(table[0x8], is),
		read_block<section_9>(table[0x9], is),
		read_block<section_a>(table[0xa], is),
		read_block<section_b>(table[0xb], is)};
}

struct MBS::data_t : public std::variant<v77, std::monostate> {
	friend std::ostream& operator<<(std::ostream&, const data_t&);
};

MBS::MBS(std::istream& is) : MBS(std::move(is)) {}

MBS::MBS(std::istream&& is)
	: _filename(2 * 16, char(0))
	, _dataptr(read(is)) {}

MBS::~MBS() noexcept { _dataptr.reset(); }

void MBS::dump() const {
	if (!_dataptr.get()) {
		return;
	}
	std::cout << *(_dataptr.get());
}

std::unique_ptr<MBS::data_t> MBS::read(std::istream& is) {
	is.seekg(0, std::ios::beg);
	const mbs_header h = read_value<mbs_header>(is);
	if (std::string_view(h.magic, sizeof(h.magic)) != mbs_header::FMBS) {
		std::cout << "Not an FMBS file." << std::endl;
		return std::make_unique<MBS::data_t>(std::monostate{});
	}

	is.seekg(0x80, std::ios::beg);
	is.read(_filename.data(), _filename.size());
	trim_string(_filename);

	switch (h.version) {
	case 0x77: {
		return std::make_unique<data_t>(handle_mbsv77(is));
		break;
	}
	case 0x66:
	case 0x6b:
	case 0x6d:
	case 0x6e:
	case 0x72:
	case 0x76:
	default: {
		std::cout << "Unsupported FMBS version " << (uint16_t)h.version << std::endl;
	}
	}
	return std::make_unique<MBS::data_t>(std::monostate{});
}

std::ostream& operator<<(std::ostream& os, const v77& v) {
	std::cout << v.s0.size() << ' ' << v.s1.size() << ' ' << v.s2.size() << ' ' << v.s3.size() << ' ' << v.s4.size()
			  << ' ' << v.s5.size() << ' ' << v.s6.size() << ' ' << v.s7.size() << ' ' << v.s8.size() << ' '
			  << v.s9.size() << ' ' << v.sa.size() << ' ' << v.sb.size() << '\n';
	return os;
}

std::ostream& operator<<(std::ostream& os, const MBS::data_t& v) {
	auto print_v77 = [](const v77& w) { std::cout << w; };
	auto print_mst = [](const std::monostate&) {};
	std::visit(overloads{print_v77, print_mst}, v);
	return os;
}
