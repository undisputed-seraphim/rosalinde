#pragma once

#include "sections.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <string_view>

namespace mbs {

using namespace std::literals;

inline void dump_csv(const v77& v, const std::filesystem::path& outdir) {
	std::filesystem::create_directories(outdir);

	auto open = [&](const char* name) {
		return std::ofstream(outdir / name);
	};

	{
		auto ofs = open("section_0.csv");
		ofs << "center,r,g,b,a,topleft\n";
		constexpr auto fmt = "{:#x},{:#x},{:#x},{:#x},{:#x},{:#x}\n"sv;
		for (const auto& s : v.s0) {
			ofs << std::format(
				fmt,
				s.center,
				static_cast<uint8_t>(s.colors[0] >> 24),
				static_cast<uint8_t>(s.colors[0] >> 16),
				static_cast<uint8_t>(s.colors[0] >> 8),
				static_cast<uint8_t>(s.colors[0] >> 0),
				s.topleft);
		}
	}

	{
		auto ofs = open("section_1.csv");
		ofs << "center_x,center_y,uv0_x,uv0_y,uv1_x,uv1_y,uv2_x,uv2_y,uv3_x,uv3_y,topleft_x,topleft_y\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s1) {
			ofs << std::format(
				fmt,
				s.center[0], s.center[1],
				s.values[0].x, s.values[0].y,
				s.values[1].x, s.values[1].y,
				s.values[2].x, s.values[2].y,
				s.values[3].x, s.values[3].y,
				s.topleft[0], s.topleft[1]);
		}
	}

	{
		auto ofs = open("section_2.csv");
		ofs << "unused_x,unused_y,v0_x,v0_y,v1_x,v1_y,v2_x,v2_y,v3_x,v3_y,topleft_x,topleft_y\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s2) {
			ofs << std::format(
				fmt,
				s.unused0[0], s.unused0[1],
				s.values[0].x, s.values[0].y,
				s.values[1].x, s.values[1].y,
				s.values[2].x, s.values[2].y,
				s.values[3].x, s.values[3].y,
				s.topleft[0], s.topleft[1]);
		}
	}

	{
		auto ofs = open("section_3.csv");
		ofs << "hb0_x,hb0_y,hb1_x,hb1_y,hb2_x,hb2_y,hb3_x,hb3_y,"
			<< "xyz0_x,xyz0_y,xyz0_z,xyz1_x,xyz1_y,xyz1_z,xyz2_x,xyz2_y,xyz2_z,xyz3_x,xyz3_y,xyz3_z\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s3) {
			ofs << std::format(
				fmt,
				s.hitbox[0].x, s.hitbox[0].y,
				s.hitbox[1].x, s.hitbox[1].y,
				s.hitbox[2].x, s.hitbox[2].y,
				s.hitbox[3].x, s.hitbox[3].y,
				s._xyz[0].x, s._xyz[0].y, s._xyz[0].z,
				s._xyz[1].x, s._xyz[1].y, s._xyz[1].z,
				s._xyz[2].x, s._xyz[2].y, s._xyz[2].z,
				s._xyz[3].x, s._xyz[3].y, s._xyz[3].z);
		}
	}

	{
		auto ofs = open("section_4.csv");
		ofs << "_unk0,color_id,flags,blend_id,tex_id,attributes,s1_id,s0_id,s2_id,_pad\n";
		constexpr auto fmt = "{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x}\n"sv;
		constexpr auto decfmt = "{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s4) {
			ofs << std::format(
				decfmt, s._unk0, s.color_id, s.flags, s.blend_id,
				s.tex_id, s.attributes, s.s1_id, s.s0_id, s.s2_id, s._pad);
		}
	}

	{
		auto ofs = open("section_5.csv");
		ofs << "s3_id,_unk0,_unk1,flags\n";
		constexpr auto fmt = "{:#x},{:#x},{:#x},{:#x}\n"sv;
		constexpr auto decfmt = "{},{},{},{}\n"sv;
		for (const auto& s : v.s5) {
			ofs << std::format(decfmt, s.s3_id, s._unk0, s._unk1, s.flags);
		}
	}

	{
		auto ofs = open("section_6.csv");
		ofs << "left,top,right,bottom,s4_id,s5_id,s4_no,s5_no,flags,_pad0\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s6) {
			ofs << std::format(
				fmt, s.left, s.top, s.right, s.bottom,
				s.s4_id, s.s5_id, s.s4_no, s.s5_no, s.flags, s._pad0);
		}
	}

	{
		auto ofs = open("section_7.csv");
		ofs << "move_x,move_y,move_z,rotate_x,rotate_y,rotate_z,scale_x,scale_y,fog\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s7) {
			ofs << std::format(
				fmt,
				s.move.x, s.move.y, s.move.z,
				s.rotate.x, s.rotate.y, s.rotate.z,
				s.scale.x, s.scale.y,
				s.fog);
		}
	}

	{
		auto ofs = open("section_8.csv");
		ofs << "s6_id,_pad0,s7_id,frames,"
			<< "FLIPX,FLIPY,JUMP,0x20,0x80,HITBOX,LAST,0x2000,"
			<< "loop_s8_id,s5s3_interp,interp_rate,s7_interp,s6_interp,s0s1s2_interp,n_180,_pad1,_pad2,sfx_mute,sfx_id\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n"sv;
		constexpr auto flagfmt = "{},{},{},{},{},{},{},{}"sv;
		for (const auto& s : v.s8) {
			const auto flagstr = std::format(
				flagfmt,
				(s.flags & 0x01 ? 1 : 0),
				(s.flags & 0x02 ? 1 : 0),
				(s.flags & 0x04 ? 1 : 0),
				(s.flags & 0x20 ? 1 : 0),
				(s.flags & 0x80 ? 1 : 0),
				(s.flags & 0x400 ? 1 : 0),
				(s.flags & 0x800 ? 1 : 0),
				(s.flags & 0x2000 ? 1 : 0));
			ofs << std::format(
				fmt,
				s.s6_id, s._pad0, s.s7_id, s.frames,
				flagstr,
				s.loop_s8_id, s.s5s3_interpolation, s.interpolation_rate,
				s.s7_interpolation, s.s6_interpolation, s.s0s1s2_interpolation,
				s.n_180, s._pad1, s._pad2, s.sfx_mute, s.sfx_id);
		}
	}

	{
		auto ofs = open("section_9.csv");
		ofs << "left,top,right,bottom,name,sa_set_id,sa_set_no,sa_set_main,sa_sb_set_id,sa_sb_set_no,disabled\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s9) {
			ofs << std::format(
				fmt,
				s.left, s.top, s.right, s.bottom,
				s.name, s.sa_set_id, s.sa_set_no, s.sa_set_main,
				s.sa_sb_set_id, s.sa_sb_set_no, s.disabled);
		}
	}

	{
		auto ofs = open("section_a.csv");
		ofs << "s8_id,s8_no,s8_sum,s8_sum_once,_unk0,sb_id,sb_no,s8_st,track_id,_pad\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.sa) {
			ofs << std::format(
				fmt,
				s.s8_id, s.s8_no, s.s8_sum, s.s8_sum_once, s._unk0,
				s.sb_id, s.sb_no, s.s8_st, s.track_id, s._pad);
		}
	}

	{
		auto ofs = open("section_b.csv");
		ofs << "_unk0,_unk1,_unk2,_unk3,_unk4,_unk5,_pad\n";
		constexpr auto fmt = "{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.sb) {
			ofs << std::format(
				fmt, s._unk0, s._unk1, s._unk2, s._unk3, s._unk4, s._unk5, s._pad);
		}
	}
}

} // namespace mbs
