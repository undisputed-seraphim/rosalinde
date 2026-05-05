#include "detail.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <istream>
#include <span>
#include <vector>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace mbs {

constexpr auto table_v77 = std::array{
	detail::lookup_entry{0x54, 4, 0xb0, 0x18}, // 0:s0  fog colors
	detail::lookup_entry{0x58, 4, 0xb8, 0x30}, // 1:s1  texture UVs
	detail::lookup_entry{0x5c, 4, 0xc0, 0x30}, // 2:s2  vertices
	detail::lookup_entry{0x60, 4, 0xc8, 0x50}, // 3:s3  hitboxes
	detail::lookup_entry{0x50, 4, 0xd0, 0x14}, // 4:s4  keyframe layers
	detail::lookup_entry{0x64, 2, 0xd8, 0x08}, // 5:s5  hitbox entries
	detail::lookup_entry{0x6a, 2, 0xe0, 0x1c}, // 6:s6  keyframes
	detail::lookup_entry{0x66, 2, 0xe8, 0x24}, // 7:s7  transforms
	detail::lookup_entry{0x68, 2, 0xf0, 0x20}, // 8:s8  animation frames
	detail::lookup_entry{0x6c, 2, 0xf8, 0x30}, // 9:s9  tracks
	detail::lookup_entry{0x6e, 2, 0x100, 0x18}, // a:sa  sequences
	detail::lookup_entry{0x72, 2, 0x108, 0x14}, // b:sb  extras
};

void parse_v77(std::span<const uint8_t> data, v77& v) {
	auto tbl = table_v77;
	byte_reader r(data);
	detail::resolve_table(r, tbl);
	detail::populate_sections(r, v, tbl);
}

// Convenience — drains the entire stream, delegates to the span path.
std::istream& operator>>(std::istream& is, v77& v) {
	is.seekg(0, std::ios::end);
	const auto size = is.tellg();
	is.seekg(0, std::ios::beg);
	std::vector<uint8_t> buffer(static_cast<size_t>(size));
	is.read(reinterpret_cast<char*>(buffer.data()), size);
	parse_v77(buffer, v);
	return is;
}

std::ostream& operator<<(std::ostream& os, const v77& v) {
	// TODO
	//print_to_stream(v);
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
		const auto flagstr = std::format(
			fmts8flag,
			(s.flags & 0x01 ? 1 : 0),
			(s.flags & 0x02 ? 1 : 0),
			(s.flags & 0x04 ? 1 : 0),
			(s.flags & 0x20 ? 1 : 0),
			(s.flags & 0x80 ? 1 : 0),
			(s.flags & 0x400 ? 1 : 0),
			(s.flags & 0x800 ? 1 : 0),
			(s.flags & 0x2000 ? 1 : 0));
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

glm::mat4 s7_matrix(const section_7& s7, const bool flipx, const bool flipy) {
	const int8_t x = flipx ? -1 : 1;
	const int8_t y = flipy ? -1 : 1;
	glm::mat4 m{1.0};
	m = glm::scale(m, glm::vec3{s7.scale.x * x, s7.scale.y * y, 1.0});
	m = glm::translate(m, glm::vec3{s7.move.x * x, s7.move.y * y, s7.move.z});
	m *= glm::eulerAngleXYZ(s7.rotate.x, s7.rotate.y, s7.rotate.z);
	// NOTE: Normally the right order for this is scale-rotate-translate,
	// however accessories seem to be wrongly placed.
	// So scale-translate-rotate appears to get us closest to the right image.
	// I think there is some parent-child transform hierarchy that's currently missing.
	return m;
}

} // namespace mbs
