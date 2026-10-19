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

	auto open = [&](const char* name) { return std::ofstream(outdir / name); };

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
				s.center[0],
				s.center[1],
				s.values[0].x,
				s.values[0].y,
				s.values[1].x,
				s.values[1].y,
				s.values[2].x,
				s.values[2].y,
				s.values[3].x,
				s.values[3].y,
				s.topleft[0],
				s.topleft[1]);
		}
	}

	{
		auto ofs = open("section_2.csv");
		ofs << "unused_x,unused_y,v0_x,v0_y,v1_x,v1_y,v2_x,v2_y,v3_x,v3_y,topleft_x,topleft_y\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s2) {
			ofs << std::format(
				fmt,
				s.unused0[0],
				s.unused0[1],
				s.values[0].x,
				s.values[0].y,
				s.values[1].x,
				s.values[1].y,
				s.values[2].x,
				s.values[2].y,
				s.values[3].x,
				s.values[3].y,
				s.topleft[0],
				s.topleft[1]);
		}
	}

	{
		auto ofs = open("section_3.csv");
		ofs << "hb0_x,hb0_y,hb1_x,hb1_y,hb2_x,hb2_y,hb3_x,hb3_y,"
			<< "n0_x,n0_y,n0_z,n1_x,n1_y,n1_z,n2_x,n2_y,n2_z,n3_x,n3_y,n3_z\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s3) {
			ofs << std::format(
				fmt,
				s.hitbox[0].x,
				s.hitbox[0].y,
				s.hitbox[1].x,
				s.hitbox[1].y,
				s.hitbox[2].x,
				s.hitbox[2].y,
				s.hitbox[3].x,
				s.hitbox[3].y,
				s.normals[0].x,
				s.normals[0].y,
				s.normals[0].z,
				s.normals[1].x,
				s.normals[1].y,
				s.normals[1].z,
				s.normals[2].x,
				s.normals[2].y,
				s.normals[2].z,
				s.normals[3].x,
				s.normals[3].y,
				s.normals[3].z);
		}
	}

	{
		auto ofs = open("section_4.csv");
		ofs << "s4_source_tile_id,color_id,flags,blend_id,tex_id,attributes,s1_uv_id,s0_color_row_id,s2_vertex_id,_pad\n";
		constexpr auto fmt = "{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x},{:#x}\n"sv;
		constexpr auto decfmt = "{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s4) {
			ofs << std::format(
				decfmt,
				s.s4_source_tile_id,
				s.color_id,
				s.flags,
				s.blend_id,
				s.tex_id,
				s.attributes,
				s.s1_uv_id,
				s.s0_color_row_id,
				s.s2_vertex_id,
				s._pad);
		}
	}

	{
		auto ofs = open("section_5.csv");
		ofs << "s3_id,s5_hitbox_kind,s5_hitbox_control,flags\n";
		constexpr auto fmt = "{:#x},{:#x},{:#x},{:#x}\n"sv;
		constexpr auto decfmt = "{},{},{},{}\n"sv;
		for (const auto& s : v.s5) {
			ofs << std::format(decfmt, s.s3_id, s.s5_hitbox_kind, s.s5_hitbox_control, s.flags);
		}
	}

	{
		auto ofs = open("section_6.csv");
		ofs << "left,top,right,bottom,s4_layer_start,s5_hitbox_start,s4_layer_count,s5_hitbox_count,flags,_pad0\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s6) {
			ofs << std::format(
				fmt, s.left, s.top, s.right, s.bottom, s.s4_layer_start, s.s5_hitbox_start, s.s4_layer_count, s.s5_hitbox_count, s.flags, s._pad0);
		}
	}

	{
		auto ofs = open("section_7.csv");
		ofs << "move_x,move_y,move_z,rotate_x,rotate_y,rotate_z,scale_x,scale_y,fog\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s7) {
			ofs << std::format(
				fmt, s.move.x, s.move.y, s.move.z, s.rotate.x, s.rotate.y, s.rotate.z, s.scale.x, s.scale.y, s.fog);
		}
	}

	{
		auto ofs = open("section_8.csv");
		ofs << "s6_keyframe_id,_pad0,s7_transform_id,s8_duration_ticks,"
			<< "FLIPX,FLIPY,JUMP,HOLD,ACTIVE,HIDDEN,EVENT,NODRAW,LAST,STOP,GLOBAL,"
			<< "s8_loop_target,s5s3_interp,interp_rate,s7_interp,s6_interp,s8_render_value,s8_draw_offset,s8_event_arg,s8_event_flag,s8_event_value,s8_reserved\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n"sv;
		constexpr auto flagfmt = "{},{},{},{},{},{},{},{},{},{},{}"sv;
		for (const auto& s : v.s8) {
			const auto flagstr = std::format(
				flagfmt,
				(s.flags & 0x01 ? 1 : 0),
				(s.flags & 0x02 ? 1 : 0),
				(s.flags & 0x04 ? 1 : 0),
				(s.flags & 0x08 ? 1 : 0),
				(s.flags & 0x20 ? 1 : 0),
				(s.flags & 0x40 ? 1 : 0),
				(s.flags & 0x80 ? 1 : 0),
				(s.flags & 0x400 ? 1 : 0),
				(s.flags & 0x800 ? 1 : 0),
				(s.flags & 0x1000 ? 1 : 0),
				(s.flags & 0x2000 ? 1 : 0));
			ofs << std::format(
				fmt,
				s.s6_keyframe_id,
				s._pad0,
				s.s7_transform_id,
				s.s8_duration_ticks,
				flagstr,
				s.s8_loop_target,
				s.s5s3_hitbox_interpolation,
				s.interpolation_rate,
				s.s7_transform_interpolation,
				s.s6_keyframe_interpolation,
				s.s8_render_value,
				s.s8_draw_offset,
				s.s8_event_arg,
				s.s8_event_flag,
				s.s8_event_value,
				s.s8_reserved);
		}
	}

	{
		auto ofs = open("section_9.csv");
		ofs << "left,top,right,bottom,name,sa_sequence_start,sa_sequence_count,sa_main_sequence,sa_sb_start,sa_sb_count,disabled\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.s9) {
			ofs << std::format(
				fmt,
				s.left,
				s.top,
				s.right,
				s.bottom,
				s.name,
				s.sa_sequence_start,
				s.sa_sequence_count,
				s.sa_main_sequence,
				s.sa_sb_start,
				s.sa_sb_count,
				s.disabled);
		}
	}

	{
		auto ofs = open("section_a.csv");
		ofs << "s8_frame_start,s8_frame_count,s8_total_ticks,s8_intro_ticks,commit_ticks,sb_id,sb_no,s8_start_offset,sa_track_slot,_pad\n";
		constexpr auto fmt = "{},{},{},{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.sa) {
			ofs << std::format(
				fmt,
				s.s8_frame_start,
				s.s8_frame_count,
				s.s8_total_ticks,
				s.s8_intro_ticks,
				s.commit_ticks,
				s.sb_id,
				s.sb_no,
				s.s8_start_offset,
				s.sa_track_slot,
				s._pad);
		}
	}

	{
		auto ofs = open("section_b.csv");
		ofs << "speed_num,speed_den,_pad0,timing_a,oneshot,timing_b,_pad1\n";
		constexpr auto fmt = "{},{},{},{},{},{},{}\n"sv;
		for (const auto& s : v.sb) {
			ofs << std::format(fmt, s.speed_num, s.speed_den, s._pad0, s.timing_a, s.oneshot, s.timing_b, s._pad1);
		}
	}
}

} // namespace mbs
