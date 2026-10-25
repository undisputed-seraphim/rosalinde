#include "sprite.hpp"

#include <criware/byte_reader.hpp>
#include <impl/mbs/detail.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>
#include <stdexcept>

namespace mbs {
glm::mat4 s7_matrix(const section_7& s7, bool flipx, bool flipy);
}

mbs::v77 parse_mbs_sections(std::span<const uint8_t> data) {
	mbs::v77 sections;
	byte_reader r(data);
	const auto header = r.read<mbs::file_header>();
	if (std::strncmp(header.magic, "FMBS", 4) != 0)
		throw std::runtime_error("Not an FMBS file.");

	switch (header.version) {
	case 0x76:
		mbs::parse_v76(data, sections);
		break;
	case 0x77:
		mbs::parse_v77(data, sections);
		break;
	default:
		throw std::runtime_error("Unsupported FMBS version.");
	}
	return sections;
}

SpriteData SpriteData::load(std::span<const uint8_t> data, std::vector<FTX::Entry> textures) {
	return build(parse_mbs_sections(data), std::move(textures));
}

SpriteData SpriteData::build(mbs::v77 sections, std::vector<FTX::Entry> textures) {
	SpriteData sd;
	sd.textures = std::move(textures);
	sd.v77 = std::move(sections);
	sd.preprocess();
	return sd;
}

void SpriteData::preprocess() {
	keyframes.resize(v77.s6.size());
	for (uint32_t s6_keyframe_id = 0; s6_keyframe_id < v77.s6.size(); ++s6_keyframe_id) {
		const auto& s6 = v77.s6[s6_keyframe_id];
		auto& ck = keyframes[s6_keyframe_id];
		ck.bounds = {s6.left, s6.top, s6.right, s6.bottom};

		ck.layers.reserve(s6.s4_layer_count);
		for (uint32_t j = 0; j < s6.s4_layer_count; ++j) {
			const auto& s4 = v77.s4[s6.s4_layer_start + j];
			const bool notex = (s4.flags & mbs::v77::s4flag::NOTEX) != 0;
			if (!notex && s4.tex_id >= textures.size())
				continue;

			CachedKeyframe::Layer layer;
			layer.notex = notex;
			layer.tex_id = notex ? 0xFFFF : s4.tex_id;
			layer.s4_layer_start = s6.s4_layer_start + j;
			layer.s4_source_tile_id = s4.s4_source_tile_id;
			layer.s4_flags = s4.flags;
			layer.s1_uv_id = s4.s1_uv_id;
			layer.s2_vertex_id = s4.s2_vertex_id;
			layer.s0_color_row_id = s4.s0_color_row_id;
			layer.color_id = s4.color_id;
			if (s4.color_id < v77.s0.size())
				for (int k = 0; k < 4; ++k)
					layer.alt_color[k] = v77.s0[s4.color_id].colors[k];
			layer.attributes = s4.attributes;
			layer.blend = s4.blend_id;
			if (!notex) {
				const auto& tex = textures[s4.tex_id];
				const auto texdim = glm::vec2{tex.width, tex.height};
				for (int k = 0; k < 4; ++k)
					layer.uv[k] = v77.s1[s4.s1_uv_id].values[k] * texdim;
				layer.s1_center = glm::vec2{v77.s1[s4.s1_uv_id].center[0], v77.s1[s4.s1_uv_id].center[1]} * texdim;
				layer.s1_topleft = glm::vec2{v77.s1[s4.s1_uv_id].topleft[0], v77.s1[s4.s1_uv_id].topleft[1]} * texdim;
			}
			for (int k = 0; k < 4; ++k) {
				layer.xy[k] = v77.s2[s4.s2_vertex_id].values[k];
				layer.color[k] = v77.s0[s4.s0_color_row_id].colors[k];
			}
			ck.layers.push_back(layer);
		}
	}

	tracks = analyze_tracks(v77);
}

std::vector<SpriteData::Track> analyze_tracks(const mbs::v77& v77) {
	std::vector<SpriteData::Track> tracks(v77.s9.size());
	for (uint32_t s9_id = 0; s9_id < v77.s9.size(); ++s9_id) {
		const auto& s9 = v77.s9[s9_id];
		auto& track = tracks[s9_id];
		track.name = s9.name;
		track.name = track.name.c_str();
		track.bounds = {s9.left, s9.top, s9.right, s9.bottom};
		track.sa_main_sequence = s9.sa_main_sequence;
		track.disabled = s9.disabled;
		if (s9.sa_sb_count > 0 && s9.sa_sb_start + s9.sa_sb_count <= v77.sb.size()) {
			const uint16_t transition = v77.sb[s9.sa_sb_start].transition_mode;
			track.transition_mode = static_cast<uint8_t>(transition);
			track.one_shot = transition >= 1 && transition <= 3;
			if (track.one_shot && s9_id + 1 < tracks.size() && !v77.s9[s9_id + 1].disabled)
				track.next_track = static_cast<int>(s9_id + 1);
			else
				track.one_shot = false;
		}
		track.runs.reserve(s9.sa_sequence_count);
		for (uint32_t i = 0; i < s9.sa_sequence_count; ++i) {
			const auto& sa = v77.sa[s9.sa_sequence_start + i];
			uint32_t loop_start = 0;
			if (sa.s8_intro_ticks > 0) {
				uint32_t acc = 0;
				for (uint32_t j = 0; j < sa.s8_frame_count; ++j) {
					acc += v77.s8[sa.s8_frame_start + j].s8_duration_ticks;
					if (acc >= (uint32_t)sa.s8_intro_ticks) {
						loop_start = j + 1;
						break;
					}
				}
			}
			// s8_intro_ticks < 0 == single-pass ("transitional"/background); play
			// through once and hold the final frame instead of looping.
			track.runs.push_back(
				{sa.s8_frame_start,
				 sa.s8_frame_count,
				 sa.s8_start_offset,
				 loop_start,
				 sa.s8_intro_ticks < 0,
				 sa.sa_track_slot});
		}
		// The action length is the one-shot intro of the main sequence; the rest
		// of the sequence loops and is what the idle/follow-up clip replaces.
		long intro_ticks = 0;
		if (!track.runs.empty()) {
			const auto& run = track.runs[std::min<size_t>(track.sa_main_sequence, track.runs.size() - 1)];
			for (uint32_t i = 0; i < run.loop_start && run.s8_start + i < v77.s8.size(); ++i)
				intro_ticks += v77.s8[run.s8_start + i].s8_duration_ticks;
		}
		track.action_frames = intro_ticks > 0 ? (intro_ticks + 1) / 2 : track_duration_frames(v77, track);
	}
	return tracks;
}

long track_duration_frames(const mbs::v77& sections, const SpriteData::Track& track) {
	long ticks = 0;
	for (const auto& run : track.runs) {
		long run_ticks = 0;
		for (uint32_t i = 0; i < run.s8_count && run.s8_start + i < sections.s8.size(); ++i)
			run_ticks += sections.s8[run.s8_start + i].s8_duration_ticks;
		ticks = std::max(ticks, run_ticks);
	}
	return ticks <= 0 ? 0 : (ticks + 1) / 2;
}

long track_duration_frames(const SpriteData& data, int track) {
	if (track < 0 || static_cast<size_t>(track) >= data.tracks.size())
		return 0;
	return track_duration_frames(data.v77, data.tracks[static_cast<size_t>(track)]);
}

const SpriteData::Track* SpriteData::find_track(const std::string& name) const {
	for (const auto& t : tracks) {
		if (t.name == name)
			return &t;
	}
	return nullptr;
}

void SpriteInstance::play(uint32_t track_id) {
	if (data)
		playback.play(*data, track_id, options);
}

void SpriteInstance::play(const std::string& name) {
	if (data)
		playback.play(*data, name, options);
}

void SpriteInstance::advance(float dt_seconds) {
	if (data)
		playback.advance(*data, dt_seconds);
}

void SpriteInstance::next_track() {
	if (data)
		playback.next_track(*data, options);
}

void SpriteInstance::prev_track() {
	if (data)
		playback.prev_track(*data, options);
}

namespace {

uint32_t scale_color(uint32_t c, const glm::vec4& m) {
	uint32_t r = ((c >> 0) & 0xFF) * m.r;
	uint32_t g = ((c >> 8) & 0xFF) * m.g;
	uint32_t b = ((c >> 16) & 0xFF) * m.b;
	uint32_t a = ((c >> 24) & 0xFF) * m.a;
	return (a << 24) | (b << 16) | (g << 8) | r;
}

} // namespace

template <typename F>
void SpriteInstance::for_each_layer(
	uint32_t sa_idx,
	const std::map<uint32_t, glm::vec4>* tints,
	bool apply_fog,
	int blend_filter,
	uint32_t layer_mask,
	F&& visit) const {
	if (!data || playback.track_idx >= data->tracks.size())
		return;

	const auto& track = data->tracks[playback.track_idx];
	LayerVisit base;
	base.run = sa_idx;
	base.transition_t = playback.transition_t;
	if (!mone && !character && track.runs.size() > 1 && track.sa_main_sequence < playback.done.size() &&
		playback.done[track.sa_main_sequence]) {
		base.skip = "main_done";
		visit(base);
		return;
	}

	if (sa_idx >= track.runs.size()) {
		base.skip = "run";
		visit(base);
		return;
	}

	const auto& run = track.runs[sa_idx];
	base.sa_track_slot = run.sa_track_slot;
	base.offset = sa_idx < playback.offsets.size() ? playback.offsets[sa_idx] : 0;
	base.done = sa_idx < playback.done.size() && playback.done[sa_idx] != 0;
	base.hidden = sa_idx < playback.hide.size() && playback.hide[sa_idx] != 0;

	if (run.s8_count == 0 || base.offset >= run.s8_count) {
		base.skip = "empty_run";
		visit(base);
		return;
	}
	if (base.hidden) {
		base.skip = "hidden";
		visit(base);
		return;
	}
	const uint32_t s8_index = run.s8_start + base.offset;
	if (s8_index >= data->v77.s8.size()) {
		base.skip = "s8";
		visit(base);
		return;
	}
	const auto& s8 = data->v77.s8[s8_index];
	base.s8_frame_start = s8_index;
	base.s8_flags = s8.flags;
	base.s8_frames = s8.s8_duration_ticks;
	base.s6_keyframe_id = s8.s6_keyframe_id;
	base.s7_transform_id = s8.s7_transform_id;
	base.s6_keyframe_interpolation = s8.s6_keyframe_interpolation;
	base.s7_transform_interpolation = s8.s7_transform_interpolation;
	const bool current = mone && sa_idx == current_sequence;
	if (!current && !(s8.flags & mbs::v77::s8flag::ACTIVE)) {
		base.skip = "inactive";
		visit(base);
		return;
	}
	if (s8.flags & mbs::v77::s8flag::HIDDEN) {
		base.skip = "s8_hidden";
		visit(base);
		return;
	}
	if (s8.flags & mbs::v77::s8flag::NODRAW) {
		base.skip = "nodraw";
		visit(base);
		return;
	}
	if (s8.s6_keyframe_id >= data->keyframes.size() || s8.s7_transform_id >= data->v77.s7.size()) {
		base.skip = "ids";
		visit(base);
		return;
	}

	const auto& ck = data->keyframes[s8.s6_keyframe_id];
	const auto& s7 = data->v77.s7[s8.s7_transform_id];
	base.s6_bounds = ck.bounds;
	base.s7_move = s7.move;
	base.s7_rotate = s7.rotate;
	base.s7_scale = s7.scale;
	base.s7_fog = s7.fog;
	if (ck.layers.empty()) {
		base.skip = "empty_keyframe";
		visit(base);
		return;
	}

	uint32_t visible = 0;
	for (const auto& layer : ck.layers) {
		if ((layer.s4_flags & mbs::v77::s4flag::SKIP) == 0 && (layer.attributes & attribute_mask) == 0 &&
			(layer.attributes & ~layer_mask) == 0)
			visible++;
	}
	const float zrate = 1.0f / (visible + 1.0f);
	float depths[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	const int source_filter = options.source_filter;

	for (size_t i = 0; i < ck.layers.size(); ++i) {
		const auto& layer = ck.layers[i];
		if (source_filter >= 0 && layer.s4_source_tile_id != static_cast<uint32_t>(source_filter))
			continue;
		LayerVisit v = base;
		v.layer = static_cast<int32_t>(i);
		v.s4_layer_start = layer.s4_layer_start;
		v.s4_source_tile_id = layer.s4_source_tile_id;
		v.s4_flags = layer.s4_flags;
		v.s1_uv_id = layer.s1_uv_id;
		v.s2_vertex_id = layer.s2_vertex_id;
		v.s0_color_row_id = layer.s0_color_row_id;
		v.color_id = layer.color_id;
		for (int k = 0; k < 4; ++k)
			v.alt_color[k] = layer.alt_color[k];
		v.tex_id = layer.tex_id;
		v.attributes = layer.attributes;
		v.blend = layer.blend;
		v.notex = layer.notex;
		// A flags=1/blend=1 layer is a mask companion only when the same
		// keyframe carries its flags=1/blend=0 source. Standalone blend=1
		// layers (opening fire, glow, and effect passes) are real draws.
		if (layer.blend == 1 && layer.s4_flags == mbs::v77::s4flag::ENABLED) {
			bool paired = false;
			for (const auto& other : ck.layers) {
				if (&other == &layer || other.blend != 0 || other.s4_flags != mbs::v77::s4flag::ENABLED)
					continue;
				if (other.s4_source_tile_id == layer.s4_source_tile_id && other.s1_uv_id == layer.s1_uv_id &&
					other.s2_vertex_id == layer.s2_vertex_id) {
					paired = true;
					break;
				}
			}
			if (!background || paired) {
				v.skip = "unflagged_mask";
				visit(v);
				continue;
			}
		}
		v.flip_v = flip_v;
		if (!layer.notex && layer.tex_id < data->textures.size()) {
			v.tex_width = data->textures[layer.tex_id].width;
			v.tex_height = data->textures[layer.tex_id].height;
		}
		v.s1_center = layer.s1_center;
		v.s1_topleft = layer.s1_topleft;
		for (int k = 0; k < 4; ++k) {
			v.uv[k] = layer.uv[k];
			if (flip_v && v.tex_height)
				v.uv[k].y = v.tex_height - v.uv[k].y;
			v.xy[k] = layer.xy[k];
			v.raw_color[k] = layer.color[k];
			v.color[k] = layer.color[k];
		}

		if ((layer.s4_flags & mbs::v77::s4flag::SKIP) != 0) {
			v.skip = "skip";
			visit(v);
			continue;
		}
		if ((layer.attributes & attribute_mask) != 0) {
			v.skip = "attribute";
			visit(v);
			continue;
		}
		if ((layer.attributes & ~layer_mask) != 0) {
			v.skip = "layer_mask";
			visit(v);
			continue;
		}
		if (layer.color_id >= 1 && layer.color_id <= 3 && (color_mask & (1u << layer.color_id)) != 0) {
			// chara_color_block_apply_preset (0x2c2e08) copies the preset's
			// colour0..colour2 into block slots 0..2 (colour3 is never copied by
			// the preset path). The layer roles below were validated against the
			// reference frame with an A/B/C/D slot-order sweep at Dr_OW_C_0
			// frame 420: color_id 1 = dress, 2 = hair, 3 = torso.
			static constexpr uint32_t preset_for_color[4] = {0, 1, 3, 0};
			const uint32_t rgb = color_palette[preset_for_color[layer.color_id]] & 0x00FFFFFFu;
			for (int k = 0; k < 4; ++k)
				v.color[k] = (layer.color[k] & 0xFF000000u) | rgb;
			// Hair can be recoloured by GradientMap00.ftx, but the LUT V
			// coordinate (g_GradientMapPartsValue) is a CPU-supplied per-unit
			// gradient value whose default we have not located yet. Until it is
			// traced, leave v.gradient at -1 so the shader keeps the flat tint.
			// The shader formula itself is exact (docs/GRADIENT_MAP.md).
		}
		if (blend_filter >= 0 && layer.blend != blend_filter)
			continue;
		// NOTEX quads are solid fills without a texture. Full-screen veils
		// (fades, backdrops, flashes) are drawn as solid quads; smaller ones
		// are masks / letterbox / palette sources consumed elsewhere, and
		// drawing them as flat fills would paint garbage over the scene.
		if (layer.notex) {
			float minx = layer.xy[0].x, maxx = layer.xy[0].x;
			float miny = layer.xy[0].y, maxy = layer.xy[0].y;
			bool achromatic = true;
			for (int k = 0; k < 4; ++k) {
				minx = std::min(minx, layer.xy[k].x);
				maxx = std::max(maxx, layer.xy[k].x);
				miny = std::min(miny, layer.xy[k].y);
				maxy = std::max(maxy, layer.xy[k].y);
				const uint32_t c = layer.color[k];
				const uint32_t r = (c >> 0) & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF;
				achromatic = achromatic && (r == g && g == b);
			}
			const bool covers_screen = minx <= -959.0f && maxx >= 959.0f && miny <= -539.0f && maxy >= 539.0f;
			if (!achromatic || !covers_screen) {
				v.skip = "notex";
				visit(v);
				continue;
			}
		}
		if (layer.blend >= 4) {
			v.skip = "blend";
			visit(v);
			continue;
		}

		if (tints) {
			auto it = tints->find(layer.attributes);
			if (it != tints->end()) {
				for (int k = 0; k < 4; ++k)
					v.color[k] = scale_color(v.color[k], it->second);
			}
		}
		if (apply_fog) {
			const uint32_t fog = s7.fog;
			const glm::vec4 f{
				((fog >> 16) & 0xFF) / 255.0f,
				((fog >> 8) & 0xFF) / 255.0f,
				(fog & 0xFF) / 255.0f,
				((fog >> 24) & 0xFF) / 255.0f};
			for (int k = 0; k < 4; ++k)
				v.color[k] = scale_color(v.color[k], f);
		}

		v.drawn = true;
		v.depth = depths[layer.blend];
		depths[layer.blend] -= zrate;
		visit(v);
	}
}

void SpriteInstance::visit_layers(
	uint32_t sa_idx,
	const std::map<uint32_t, glm::vec4>* tints,
	bool apply_fog,
	uint32_t layer_mask,
	const LayerVisitor& visitor) const {
	for_each_layer(sa_idx, tints, apply_fog, -1, layer_mask, [&](const LayerVisit& layer) { visitor(layer); });
}

void SpriteInstance::build_vertices(
	uint32_t sa_idx,
	std::vector<SpriteVertex>& verts,
	std::vector<uint32_t>& indices,
	const std::map<uint32_t, glm::vec4>* tints,
	bool apply_fog,
	int blend_filter,
	uint32_t layer_mask) const {
	verts.clear();
	indices.clear();

	uint32_t base = 0;
	for_each_layer(sa_idx, tints, apply_fog, blend_filter, layer_mask, [&](const LayerVisit& layer) {
		if (!layer.drawn)
			return;

		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[0], {layer.xy[0], layer.depth}, layer.color[0], layer.gradient});
		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[1], {layer.xy[1], layer.depth}, layer.color[1], layer.gradient});
		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[2], {layer.xy[2], layer.depth}, layer.color[2], layer.gradient});
		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[3], {layer.xy[3], layer.depth}, layer.color[3], layer.gradient});

		indices.insert(indices.end(), {base, base + 1, base + 3, base + 1, base + 2, base + 3});
		base += 4;
	});
}

glm::mat4 SpriteInstance::transform_for_sa(uint32_t sa_idx, bool* out_flipx, bool* out_flipy) const {
	if (!data)
		return glm::mat4{1.0f};
	const glm::mat4 matrix = playback.transform_for_sa(*data, sa_idx);
	if (out_flipx || out_flipy) {
		const auto& track = data->tracks[playback.track_idx];
		const auto& run = track.runs[sa_idx];
		const auto& s8 = data->v77.s8[run.s8_start + playback.offsets[sa_idx]];
		if (out_flipx)
			*out_flipx = (s8.flags & mbs::v77::s8flag::FLIPX) != 0;
		if (out_flipy)
			*out_flipy = (s8.flags & mbs::v77::s8flag::FLIPY) != 0;
	}
	return matrix;
}

uint32_t SpriteInstance::sa_count() const { return data ? playback.sa_count(*data) : 0; }

std::vector<SpriteInstance::RunState> SpriteInstance::run_states() const {
	return data ? playback.run_states(*data) : std::vector<RunState>{};
}

uint32_t SpriteInstance::frame_counter() const { return playback.frame_counter_value(); }

glm::vec4 SpriteInstance::track_bounds() const { return data ? data->tracks[playback.track_idx].bounds : glm::vec4{}; }

glm::vec4 SpriteInstance::content_bounds() const {
	float l = INFINITY, t = INFINITY, r = -INFINITY, b = -INFINITY;
	const auto& track = data->tracks[playback.track_idx];
	for (size_t i = 0; i < track.runs.size(); ++i) {
		const auto& run = track.runs[i];
		if (run.s8_count == 0 || i >= playback.offsets.size())
			continue;
		const auto& s8 = data->v77.s8[run.s8_start + playback.offsets[i]];
		if (s8.flags & (mbs::v77::s8flag::HIDDEN | mbs::v77::s8flag::NODRAW))
			continue;
		const glm::mat4 m = mbs::s7_matrix(data->v77.s7[s8.s7_transform_id], false, false);
		const glm::vec4 kb = data->keyframes[s8.s6_keyframe_id].bounds;
		const glm::vec2 corners[4] = {{kb.x, kb.y}, {kb.z, kb.y}, {kb.x, kb.w}, {kb.z, kb.w}};
		for (const auto& c : corners) {
			const glm::vec4 p = m * glm::vec4{c.x, c.y, 0.0f, 1.0f};
			l = std::min(l, p.x);
			t = std::min(t, p.y);
			r = std::max(r, p.x);
			b = std::max(b, p.y);
		}
	}
	return {l, t, r, b};
}

glm::vec4 SpriteInstance::render_bounds() const {
	float l = INFINITY, t = INFINITY, r = -INFINITY, b = -INFINITY;
	const auto& track = data->tracks[playback.track_idx];
	for (uint32_t i = 0; i < track.runs.size(); ++i) {
		const auto& run = track.runs[i];
		if (run.s8_count == 0 || i >= playback.offsets.size())
			continue;
		const auto& s8 = data->v77.s8[run.s8_start + playback.offsets[i]];
		if (s8.flags & (mbs::v77::s8flag::HIDDEN | mbs::v77::s8flag::NODRAW))
			continue;
		if ((s8.flags & mbs::v77::s8flag::LAST) && !(s8.flags & mbs::v77::s8flag::ACTIVE))
			continue;
		// Skip heavily-scaled runs (light shafts, insert flashes): they are
		// overlays and would otherwise blow up the framing bounds.
		const auto& s7 = data->v77.s7[s8.s7_transform_id];
		if (std::max(std::abs(s7.scale.x), std::abs(s7.scale.y)) > 4.0f)
			continue;
		const auto& ck = data->keyframes[s8.s6_keyframe_id];
		if (ck.layers.empty())
			continue;

		const glm::mat4 m = transform_for_sa(i);
		for (const auto& layer : ck.layers) {
			if ((layer.s4_flags & mbs::v77::s4flag::SKIP) != 0)
				continue;
			if ((layer.attributes & attribute_mask) != 0)
				continue;
			for (int k = 0; k < 4; ++k) {
				const glm::vec4 p = m * glm::vec4{layer.xy[k].x, layer.xy[k].y, 0.0f, 1.0f};
				l = std::min(l, p.x);
				t = std::min(t, p.y);
				r = std::max(r, p.x);
				b = std::max(b, p.y);
			}
		}
	}
	return {l, t, r, b};
}
