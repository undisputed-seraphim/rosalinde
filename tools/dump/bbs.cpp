#include "dump.hpp"

#include <impl/bbs.hpp>

#include <algorithm>
#include <cstdint>
#include <format>
#include <iostream>
#include <iterator>
#include <span>
#include <string>

namespace dump {
namespace {

std::span<const uint8_t> as_bytes(std::span<char> data) {
	return {reinterpret_cast<const uint8_t*>(data.data()), data.size()};
}

size_t row_limit(const options& opt, size_t rows) {
	if (opt.limit_set)
		return std::min(opt.limit, rows);
	if (opt.full)
		return rows;
	return std::min<size_t>(rows, 10);
}

template <size_t N>
std::string hex_bytes(const std::array<uint8_t, N>& bytes) {
	std::string result;
	result.reserve(bytes.size() * 2);
	for (uint8_t byte : bytes)
		std::format_to(std::back_inserter(result), "{:02x}", byte);
	return result;
}

} // namespace

int dump_bbs(const context& ctx, std::span<char> data) {
	auto parsed = bbs::parse(as_bytes(data));
	if (!parsed) {
		std::cerr << "d: " << ctx.path << ": not a valid BBS file\n";
		return 1;
	}
	const auto& file = *parsed;

	const size_t color_limit = row_limit(ctx.opt, file.color_keys.size());
	const size_t object_limit = row_limit(ctx.opt, file.objects.size());
	const size_t camera_limit = row_limit(ctx.opt, file.cameras.size());
	const size_t light_limit = row_limit(ctx.opt, file.lights.size());
	const size_t fog_limit = row_limit(ctx.opt, file.fog.size());
	const size_t environment_limit = row_limit(ctx.opt, file.environment.size());
	const size_t material_limit = row_limit(ctx.opt, file.materials.size());

	if (ctx.opt.json) {
		json_writer json(std::cout);
		json.begin_object();
		write_metadata_json(json, ctx, "bbs");
		json.field("version", file.hdr.version);
		json.field("mbs_count", file.mbs_names.size());
		json.field("ftx_count", file.ftx_names.size());
		json.field("color_key_count", file.color_keys.size());
		json.field("object_count", file.objects.size());
		json.field("camera_count", file.cameras.size());
		json.field("light_count", file.lights.size());
		json.field("fog_count", file.fog.size());
		json.field("environment_count", file.environment.size());
		json.field("material_count", file.materials.size());
		json.begin_array("mbs_names");
		for (const auto& name : file.mbs_names)
			json.string(name);
		json.end_array();
		json.begin_array("ftx_names");
		for (const auto& name : file.ftx_names)
			json.string(name);
		json.end_array();

		if (color_limit != file.color_keys.size())
			json.field("color_keys_truncated", true);
		json.begin_array("color_keys");
		for (size_t i = 0; i < color_limit; ++i) {
			const auto& key = file.color_keys[i];
			json.begin_object();
			json.field("index", i);
			json.field("r", key.r);
			json.field("g", key.g);
			json.field("b", key.b);
			json.field("flags", key.flags);
			json.end_object();
		}
		json.end_array();

		if (object_limit != file.objects.size())
			json.field("objects_truncated", true);
		json.begin_array("objects");
		for (size_t i = 0; i < object_limit; ++i) {
			const auto& object = file.objects[i];
			json.begin_object();
			json.field("index", i);
			json.field("mbs_index", object.mbs_index);
			json.field("mbs_variant", object.mbs_variant);
			json.field("track_index", object.track_index);
			json.field("layer_index", object.layer_index);
			json.field("x", object.x);
			json.field("y", object.y);
			json.field("z", object.z);
			json.field("rotation_x", object.rotation_x);
			json.field("rotation_y", object.rotation_y);
			json.field("rotation_z", object.rotation_z);
			json.field("scale_x", object.scale_x);
			json.field("scale_y", object.scale_y);
			json.field("color_key", object.color_key);
			json.field("color", object.color);
			json.field("alpha", object.alpha);
			json.field("flags", object.flags);
			json.end_object();
		}
		json.end_array();

		const auto write_raw = [&](const char* name, const auto& records, size_t limit) {
			if (limit != records.size())
				json.field(std::format("{}_truncated", name), true);
			json.begin_array(name);
			for (size_t i = 0; i < limit; ++i) {
				json.begin_object();
				json.field("index", i);
				json.field("hex", hex_bytes(records[i].data));
				json.end_object();
			}
			json.end_array();
		};
		write_raw("cameras", file.cameras, camera_limit);
		write_raw("lights", file.lights, light_limit);
		write_raw("fog", file.fog, fog_limit);
		write_raw("environment", file.environment, environment_limit);
		write_raw("materials", file.materials, material_limit);
		json.end_object();
		std::cout << '\n';
		return 0;
	}

	write_metadata_text(ctx, "bbs");
	std::cout << "version=" << file.hdr.version << " mbs=" << file.mbs_names.size()
			  << " ftx=" << file.ftx_names.size() << " color_keys=" << file.color_keys.size()
			  << " objects=" << file.objects.size() << " cameras=" << file.cameras.size()
			  << " lights=" << file.lights.size() << " fog=" << file.fog.size()
			  << " environment=" << file.environment.size() << " materials=" << file.materials.size() << '\n';
	std::cout << "mbs_names:\n";
	for (size_t i = 0; i < file.mbs_names.size(); ++i)
		std::cout << "  [" << i << "] " << file.mbs_names[i] << '\n';
	std::cout << "ftx_names:\n";
	for (size_t i = 0; i < file.ftx_names.size(); ++i)
		std::cout << "  [" << i << "] " << file.ftx_names[i] << '\n';

	std::cout << "color_keys:\n";
	for (size_t i = 0; i < color_limit; ++i) {
		const auto& key = file.color_keys[i];
		std::cout << "  [" << i << "] r=" << key.r << " g=" << key.g << " b=" << key.b
				  << " flags=" << key.flags << '\n';
	}
	if (color_limit != file.color_keys.size())
		std::cout << "  ... " << (file.color_keys.size() - color_limit) << " more\n";

	std::cout << "objects:\n";
	for (size_t i = 0; i < object_limit; ++i) {
		const auto& object = file.objects[i];
		std::cout << "  [" << i << "] mbs=" << object.mbs_index << " variant=" << object.mbs_variant
				  << " track=" << object.track_index << " layer=" << object.layer_index << " pos=(" << object.x << ","
				  << object.y << "," << object.z << ") rot=(" << object.rotation_x << "," << object.rotation_y << ","
				  << object.rotation_z << ") scale=(" << object.scale_x << "," << object.scale_y << ")"
				  << " color_key=" << object.color_key << " color=" << object.color << " alpha=" << object.alpha
				  << " flags=" << object.flags << '\n';
	}
	if (object_limit != file.objects.size())
		std::cout << "  ... " << (file.objects.size() - object_limit) << " more\n";

	const auto write_raw = [&](const char* name, const auto& records, size_t limit) {
		std::cout << name << ":\n";
		for (size_t i = 0; i < limit; ++i)
			std::cout << "  [" << i << "] " << hex_bytes(records[i].data) << '\n';
		if (limit != records.size())
			std::cout << "  ... " << (records.size() - limit) << " more\n";
	};
	write_raw("cameras", file.cameras, camera_limit);
	write_raw("lights", file.lights, light_limit);
	write_raw("fog", file.fog, fog_limit);
	write_raw("environment", file.environment, environment_limit);
	write_raw("materials", file.materials, material_limit);
	return 0;
}

} // namespace dump
