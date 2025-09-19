#include "sections.hpp"
#include "../../utils.hpp"
#include <array>
#include <format>
#include <fstream>
#include <ostream>

namespace mbs {

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

std::istream& operator>>(std::istream& is, v77& v) {
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

	v.s0 = read_block<section_0>(table[0x0], is);
	v.s1 = read_block<section_1>(table[0x1], is);
	v.s2 = read_block<section_2>(table[0x2], is);
	v.s3 = read_block<section_3>(table[0x3], is);
	v.s4 = read_block<section_4>(table[0x4], is);
	v.s5 = read_block<section_5>(table[0x5], is);
	v.s6 = read_block<section_6>(table[0x6], is);
	v.s7 = read_block<section_7>(table[0x7], is);
	v.s8 = read_block<section_8>(table[0x8], is);
	v.s9 = read_block<section_9>(table[0x9], is);
	v.sa = read_block<section_a>(table[0xa], is);
	v.sb = read_block<section_b>(table[0xb], is);
	return is;
}

std::ostream& operator<<(std::ostream& os, const v77& v) {
	// TODO
	//print_to_file(v);
	return os;
}

using namespace std::literals;

void print_to_stream(const v77& v) {
	auto ofs = std::ofstream("section_4.csv");
	ofs << "unk0,flags,tex_id,attributes\n";
	constexpr auto fmts4 = "{},{},{},{}\n"sv;
	constexpr auto fmts4_hex = "{:#x},{:#x},{:#x},{:#x}\n"sv;
	for (const auto& s : v.s4) {
		ofs << std::format(fmts4, s._unk0, s.flags, s.tex_id, s.attributes);
	}

	ofs = std::ofstream("section_5.csv");
	ofs << "s3_id,unk0,unk1,flags\n";
	constexpr auto fmts5 = "{},{},{},{}\n"sv;
	constexpr auto fmts5_hex = "{:#x},{:#x},{:#x},{:#x}\n"sv;
	for (const auto& s : v.s5) {
		ofs << std::format(fmts5, s.s3_id, s._unk0, s._unk1, s.flags);
	}

	ofs = std::ofstream("section_6.csv");
	ofs << "s4_id,s5_id,s4_no,s5_no,flags,_pad0\n";
	constexpr auto fmts6 = "{},{},{},{},{},{}\n"sv;
	constexpr auto fmts6_hex = "{:#x},{:#x},{:#x},{:#x},{:#x},{:#x}\n"sv;
	for (const auto& s : v.s6) {
		ofs << std::format(fmts6, s.s4_id, s.s5_id, s.s4_no, s.s5_no, s.flags, s._pad0);
	}

	ofs = std::ofstream("section_8.csv");
	ofs << "s6_id,_pad0,s7_id,frames,"
		<< "FLIPX,FLIPY,JUMP,0x20,0x80,HITBOX,LAST,0x2000," // flags
		<< "loop_s8_id,s5s3_interpolation,interpolation_rate,s7_interpolation,s6_"
		   "interpolation,s0s1s2_interpolation,n_180,_pad1,_pad2,sfx_mute,sfx_id\n"
		<< std::hex;
	constexpr auto fmts8 = "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n"sv;
	constexpr auto fmts8_hex =
		"{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x}\n"sv;
	constexpr auto fmts8flag = "{},{},{},{},{},{},{},{}";
	for (const auto& s : v.s8) {
		const auto flagstr = std::format(fmts8flag,
			(s.flags & 0x01 ? 1 : 0),
			(s.flags & 0x02 ? 1 : 0),
			(s.flags & 0x04 ? 1 : 0),
			(s.flags & 0x20 ? 1 : 0),
			(s.flags & 0x80 ? 1 : 0),
			(s.flags & 0x400 ? 1 : 0),
			(s.flags & 0x800 ? 1 : 0),
			(s.flags & 0x2000 ? 1 : 0)
		);
		ofs << std::format(
			fmts8,
			s.s6_id,
			s._pad0,
			s.s7_id,
			s.frames,
			flagstr,
			s.loop_s8_id,
			s.s5s3_interpolation,
			s.interpolation_rate,
			s.s7_interpolation,
			s.s6_interpolation,
			s.s0s1s2_interpolation,
			s.n_180,
			s._pad1,
			s._pad2,
			s.sfx_mute,
			s.sfx_id);
	}

	ofs = std::ofstream("section_9.csv");
	ofs << "name,sa_set_id,sa_set_no,sa_set_main,sa_sb_set_id,sa_sb_set_no,_0_1\n";
	constexpr auto fmts9 = "{},{},{},{},{},{},{}\n"sv;
	constexpr auto fmts9_hex = "{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x}\n"sv;
	for (const auto& s : v.s9) {
		ofs << std::format(
			fmts9, s.name, s.sa_set_id, s.sa_set_no, s.sa_set_main, s.sa_sb_set_id, s.sa_sb_set_no, s.disabled);
	}

	ofs = std::ofstream("section_a.csv");
	ofs << "s8_id,s8_no,s8_sum,s8_sumonce,unk0,sb_id,sb_no,s8_st,unk1,unk2\n";
	constexpr auto fmtsa = "{},{},{},{},{},{},{},{},{},{}\n"sv;
	constexpr auto fmtsa_hex = "{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x}\n"sv;
	for (const auto& s : v.sa) {
		ofs << std::format(
			fmtsa, s.s8_id, s.s8_no, s.s8_sum, s.s8_sum_once, s._unk0, s.sb_id, s.sb_no, s.s8_st, s.track_id, s._pad);
	}

	ofs = std::ofstream("section_b.csv");
	ofs << "col0,col1,col2,col3,col4,col5,pad\n";
	constexpr auto fmtsb = "{},{},{},{},{},{},{}\n"sv;
	constexpr auto fmtsb_hex = "{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x}\n"sv;
	for (const auto& s : v.sb) {
		ofs << std::format(fmtsb, s._unk0, s._unk1, s._unk2, s._unk3, s._unk4, s._unk5, s._pad);
	}
}


} // namespace mbs