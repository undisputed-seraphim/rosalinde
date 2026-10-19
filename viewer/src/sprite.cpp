#include "sprite.hpp"

#include <criware/byte_reader.hpp>
#include <impl/mbs/detail.hpp>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <span>
#include <stdexcept>

namespace mbs {
glm::mat4 s7_matrix(const section_7& s7, bool flipx, bool flipy);
}

namespace {
// How a finished single-pass (s8_intro_ticks < 0) sequence behaves:
// 0 = hide the part (default), 1 = hold its final frame, 2 = loop (legacy).
int once_mode() {
	static const int m = [] {
		const char* e = std::getenv("VIEWER_ONCE");
		if (e && std::strcmp(e, "hold") == 0)
			return 1;
		if (e && std::strcmp(e, "loop") == 0)
			return 2;
		return 0;
	}();
	return m;
}

// A track's runs are either parallel parts (default) or sequential phases
// (VIEWER_RUNS=seq): in the latter only the current phase draws and advances.
bool seq_runs() {
	static const bool on = std::getenv("VIEWER_RUNS") && std::strcmp(std::getenv("VIEWER_RUNS"), "seq") == 0;
	return on;
}
} // namespace

static glm::mat4 s7_matrix_interp(const mbs::section_7& a, const mbs::section_7& b, float t, bool flipx, bool flipy) {
	const int8_t x = flipx ? -1 : 1;
	const int8_t y = flipy ? -1 : 1;
	glm::vec3 move = glm::mix(a.move, b.move, t);
	glm::vec3 rotate = glm::mix(a.rotate, b.rotate, t);
	glm::vec2 scale = glm::mix(a.scale, b.scale, t);
	glm::mat4 m{1.0};
	m = glm::scale(m, glm::vec3{scale.x * x, scale.y * y, 1.0});
	m = glm::translate(m, glm::vec3{move.x * x, move.y * y, move.z});
	m *= glm::eulerAngleXYZ(rotate.x, rotate.y, rotate.z);
	return m;
}

SpriteData SpriteData::load(std::span<const uint8_t> data, std::vector<FTX::Entry> textures) {
	SpriteData sd;
	sd.textures = std::move(textures);

	byte_reader r(data);
	const auto h = r.read<mbs::file_header>();
	if (std::strncmp(h.magic, "FMBS", 4) != 0) {
		throw std::runtime_error("Not an FMBS file.");
	}

	switch (h.version) {
	case 0x76:
		mbs::parse_v76(data, sd.v77);
		break;
	case 0x77:
		mbs::parse_v77(data, sd.v77);
		break;
	default:
		throw std::runtime_error("Unsupported FMBS version.");
	}

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
			const bool notex = (s4.flags & 0x04) != 0;
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

	for (const auto& s4 : v77.s4) {
		attributes_mask |= s4.attributes;
	}
	for (uint32_t i = 0; i < 32; ++i) {
		if (attributes_mask & (1u << i))
			attribute_bits.push_back(1u << i);
	}

	tracks.resize(v77.s9.size());
	for (uint32_t s9_id = 0; s9_id < v77.s9.size(); ++s9_id) {
		const auto& s9 = v77.s9[s9_id];
		auto& track = tracks[s9_id];
		track.name = s9.name;
		track.name = track.name.c_str();
		track.bounds = {s9.left, s9.top, s9.right, s9.bottom};
		track.sa_main_sequence = s9.sa_main_sequence;
		track.disabled = s9.disabled;
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
				{sa.s8_frame_start, sa.s8_frame_count, sa.s8_start_offset, loop_start, sa.s8_intro_ticks < 0 && once_mode() != 2, sa.sa_track_slot});
		}
	}
}

const SpriteData::Track* SpriteData::find_track(const std::string& name) const {
	for (const auto& t : tracks) {
		if (t.name == name)
			return &t;
	}
	return nullptr;
}

void SpriteInstance::play(uint32_t track_id) {
	if (data && track_idx < data->tracks.size() && track_idx != track_id) {
		uint32_t n = sa_count();
		_transition_src_s7.resize(n);
		for (uint32_t i = 0; i < n; ++i) {
			const auto& run = data->tracks[track_idx].runs[i];
			if (run.s8_count == 0)
				continue;
			const auto& s8 = data->v77.s8[run.s8_start + offsets[i]];
			_transition_src_s7[i] = data->v77.s7[s8.s7_transform_id];
		}
		_transition_t = 0.0f;
	}

	track_idx = track_id;
	const auto& track = data->tracks[track_idx];
	ticks.assign(track.runs.size(), 0);
	offsets.resize(track.runs.size());
	prev_offsets.assign(track.runs.size(), UINT32_MAX);
	last_visible_offsets.assign(track.runs.size(), UINT32_MAX);
	done.assign(track.runs.size(), 0);
	hide.assign(track.runs.size(), 0);
	if (std::getenv("VIEWER_MAIN_ONLY")) {
		for (uint32_t i = 0; i < hide.size(); ++i)
			hide[i] = i != track.sa_main_sequence;
	}
	if (const char* only = std::getenv("VIEWER_RUN_ONLY")) {
		const int k = std::atoi(only);
		for (uint32_t i = 0; i < hide.size(); ++i)
			hide[i] = static_cast<int>(i) != k;
	}
	if (const char* from = std::getenv("VIEWER_RUN_FROM")) {
		const int k = std::atoi(from);
		for (uint32_t i = 0; i < hide.size(); ++i)
			hide[i] = static_cast<int>(i) < k;
	}
	_phase = 0;
	if (seq_runs() && std::getenv("VIEWER_SEQ_MAIN"))
		_phase = std::min<uint32_t>(track.sa_main_sequence, track.runs.size() ? track.runs.size() - 1 : 0);
	_accum = 0.0f;

	for (uint32_t i = 0; i < track.runs.size(); ++i) {
		const auto& run = track.runs[i];
		offsets[i] = run.s8_start_offset;
		if (run.s8_count > 0 && run.s8_start < data->v77.s8.size()) {
			ticks[i] = data->v77.s8[run.s8_start + offsets[i]].s8_duration_ticks;
		}
	}
	_frame_counter++;
	if (const char* adv = std::getenv("VIEWER_ADVANCE"))
		advance_frames(static_cast<uint32_t>(std::max(0, std::atoi(adv))));
	spdlog::debug("anim: play track {} \"{}\"", track_id, track.name);
}

void SpriteInstance::play(const std::string& name) {
	for (uint32_t i = 0; i < data->tracks.size(); ++i) {
		if (data->tracks[i].name == name) {
			play(i);
			return;
		}
	}
}

void SpriteInstance::update(float dt_seconds) {
	// MBS animation sequences advance at 120 frames/s (script Wait/time units are
	// 60 Hz). Confirmed against the opening reference: a 556-frame logo sequence
	// ends at 4.6 s, and the title's flash phase lands at 14 s.
	constexpr float kTicksPerSecond = 120.0f;
	constexpr float kTransitionDuration = 0.15f;
	_accum += dt_seconds * kTicksPerSecond;

	if (_transition_t < 1.0f) {
		_transition_t += dt_seconds / kTransitionDuration;
		if (_transition_t > 1.0f)
			_transition_t = 1.0f;
	}

	bool advanced = false;
	const auto& track = data->tracks[track_idx];
	auto can_hold = [&](uint32_t run_idx, uint32_t offset) {
		const auto& run = track.runs[run_idx];
		if (run.s8_start + offset >= data->v77.s8.size())
			return false;
		const auto& s8 = data->v77.s8[run.s8_start + offset];
		if (s8.s6_keyframe_id >= data->keyframes.size())
			return false;
		const auto& ck = data->keyframes[s8.s6_keyframe_id];
		const float width = std::abs(ck.bounds.z - ck.bounds.x);
		const float height = std::abs(ck.bounds.w - ck.bounds.y);
		if (width > 3000.0f || height > 3000.0f)
			return false;
		bool textured = false;
		for (const auto& layer : ck.layers) {
			if (!layer.notex)
				textured = true;
		}
		return textured;
	};

	while (_accum >= 1.0f) {
		_accum -= 1.0f;
		const uint32_t first = seq_runs() ? std::min<uint32_t>(_phase, track.runs.size()) : 0;
		const uint32_t last = seq_runs() ? std::min<uint32_t>(_phase + 1, track.runs.size()) : track.runs.size();
		for (uint32_t i = first; i < last; ++i) {
			const auto& run = track.runs[i];
			if (run.s8_count == 0 || (i < done.size() && done[i]) || offsets[i] >= run.s8_count)
				continue;
			const auto& current_s8 = data->v77.s8[run.s8_start + offsets[i]];
			if ((current_s8.flags & mbs::v77::s8flag::ACTIVE) &&
				((data->v77.s7[current_s8.s7_transform_id].fog >> 24) != 0))
				last_visible_offsets[i] = offsets[i];
			if (--ticks[i] == 0) {
				advanced = true;
				const auto& prev_s8 = current_s8;
				prev_offsets[i] = offsets[i];
				if (prev_s8.flags & mbs::v77::s8flag::JUMP) {
					const int32_t target =
						static_cast<int32_t>(run.s8_start_offset) + static_cast<int16_t>(prev_s8.s8_loop_target);
					if (target >= 0 && static_cast<uint32_t>(target) < run.s8_count) {
						offsets[i] = static_cast<uint32_t>(target);
					} else {
						done[i] = 1;
						offsets[i] = run.s8_count - 1;
					}
				} else if (run.once && offsets[i] + 1 >= run.s8_count) {
					done[i] = 1;
					offsets[i] = run.s8_count - 1;
				} else {
					++offsets[i];
					if (offsets[i] >= run.s8_count)
						offsets[i] = run.loop_start;
				}
				// Background layers are static plates: hold their final frame.
				// Sprite runs hold their final pose while it is still visible
				// (ACTIVE + non-zero fog alpha); frames that fade to transparent
				// or clear ACTIVE terminate the run and stop drawing.
				if (done[i]) {
					const auto& end = data->v77.s8[run.s8_start + offsets[i]];
					const uint32_t fog = data->v77.s7[end.s7_transform_id].fog;
					const bool visible_end = (end.flags & mbs::v77::s8flag::ACTIVE) && ((fog >> 24) != 0);
					const bool one_tick_last =
						run.once && !hold_finished && end.s8_duration_ticks <= 1 && (end.flags & mbs::v77::s8flag::LAST);
					if (one_tick_last && track.runs.size() > 1 && last_visible_offsets[i] != UINT32_MAX &&
						can_hold(i, last_visible_offsets[i])) {
						offsets[i] = last_visible_offsets[i];
						hide[i] = 0;
					} else if (one_tick_last && prev_offsets[i] != UINT32_MAX) {
						const auto& prev = data->v77.s8[run.s8_start + prev_offsets[i]];
						const uint32_t prev_fog = data->v77.s7[prev.s7_transform_id].fog;
						const bool prev_visible = (prev.flags & mbs::v77::s8flag::ACTIVE) && ((prev_fog >> 24) != 0);
						if (prev_visible) {
							offsets[i] = prev_offsets[i];
							hide[i] = 0;
						} else {
							hide[i] = 1;
						}
					} else {
						hide[i] = (hold_finished || visible_end) ? 0 : 1;
					}
				}
				ticks[i] = data->v77.s8[run.s8_start + offsets[i]].s8_duration_ticks;
			}
		}
		// Sequential-runs mode: once a phase finishes, start the next one.
		if (seq_runs() && _phase < track.runs.size() && done[_phase] && _phase + 1 < track.runs.size()) {
			++_phase;
			const auto& r = track.runs[_phase];
			offsets[_phase] = r.s8_start_offset;
			if (r.s8_count && r.s8_start < data->v77.s8.size())
				ticks[_phase] = data->v77.s8[r.s8_start + offsets[_phase]].s8_duration_ticks;
			hide[_phase] = 0;
			advanced = true;
		}
	}

	if (advanced)
		_frame_counter++;
}

void SpriteInstance::advance_frames(uint32_t frames) {
	if (paused || !data || frames == 0)
		return;
	// `frames` are 60 Hz display frames; update() converts to 120 Hz MBS ticks.
	constexpr float kFrameSeconds = 1.0f / 60.0f;
	for (uint32_t i = 0; i < frames; ++i)
		update(kFrameSeconds);
}

void SpriteInstance::next_track() {
	if (data->tracks.empty())
		return;
	play((track_idx + 1) % data->tracks.size());
}

void SpriteInstance::prev_track() {
	if (data->tracks.empty())
		return;
	play(track_idx == 0 ? data->tracks.size() - 1 : track_idx - 1);
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
	if (!data || track_idx >= data->tracks.size())
		return;

	const auto& track = data->tracks[track_idx];
	LayerVisit base;
	base.run = sa_idx;
	base.transition_t = _transition_t;
	if (track.runs.size() > 1 && track.sa_main_sequence < done.size() && done[track.sa_main_sequence]) {
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
	base.offset = sa_idx < offsets.size() ? offsets[sa_idx] : 0;
	base.done = sa_idx < done.size() && done[sa_idx] != 0;
	base.hidden = sa_idx < hide.size() && hide[sa_idx] != 0;

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
	if (seq_runs() && sa_idx != _phase) {
		base.skip = "phase";
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
	if (!(s8.flags & mbs::v77::s8flag::ACTIVE)) {
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
		if ((layer.attributes & ~variant_flags) == 0 && (layer.attributes & ~layer_mask) == 0)
			visible++;
	}
	const float zrate = 1.0f / (visible + 1.0f);
	float depths[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	static const int source_filter = [] {
		const char* e = std::getenv("VIEWER_S4_SOURCE");
		return e ? std::atoi(e) : -1;
	}();

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
		// Opening010 pairs the colored title (flags=1, blend=0) with a white
		// silhouette of the same logo (flags=1, blend=1). The white layer is a
		// mask companion; compositing it additively erases the colored title.
		if (layer.blend == 1 && layer.s4_flags == 1) {
			v.skip = "unflagged_mask";
			visit(v);
			continue;
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

		if ((layer.attributes & ~variant_flags) != 0) {
			v.skip = "variant";
			visit(v);
			continue;
		}
		if ((layer.attributes & ~layer_mask) != 0) {
			v.skip = "layer_mask";
			visit(v);
			continue;
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

		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[0], {layer.xy[0], layer.depth}, layer.color[0]});
		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[1], {layer.xy[1], layer.depth}, layer.color[1]});
		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[2], {layer.xy[2], layer.depth}, layer.color[2]});
		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[3], {layer.xy[3], layer.depth}, layer.color[3]});

		indices.insert(indices.end(), {base, base + 1, base + 3, base + 1, base + 2, base + 3});
		base += 4;
	});
}

glm::mat4 SpriteInstance::transform_for_sa(uint32_t sa_idx, bool* out_flipx, bool* out_flipy) const {
	const auto& track = data->tracks[track_idx];
	const auto& run = track.runs[sa_idx];
	const auto& s8 = data->v77.s8[run.s8_start + offsets[sa_idx]];

	bool flipx = s8.flags & mbs::v77::s8flag::FLIPX;
	bool flipy = s8.flags & mbs::v77::s8flag::FLIPY;
	if (out_flipx)
		*out_flipx = flipx;
	if (out_flipy)
		*out_flipy = flipy;

	const auto& curr_s7 = data->v77.s7[s8.s7_transform_id];

	if (_transition_t < 1.0f && !_transition_src_s7.empty()) {
		uint32_t src_idx = sa_idx < _transition_src_s7.size() ? sa_idx : (_transition_src_s7.size() - 1);
		float t = glm::smoothstep(0.0f, 1.0f, _transition_t);
		return s7_matrix_interp(_transition_src_s7[src_idx], curr_s7, t, flipx, flipy);
	}

	if (s8.s7_transform_interpolation == 0 || prev_offsets[sa_idx] == UINT32_MAX) {
		return mbs::s7_matrix(curr_s7, flipx, flipy);
	}

	const auto& prev_s8 = data->v77.s8[run.s8_start + prev_offsets[sa_idx]];
	const auto& prev_s7 = data->v77.s7[prev_s8.s7_transform_id];
	float t = 1.0f - (float)ticks[sa_idx] / (float)s8.s8_duration_ticks;
	return s7_matrix_interp(prev_s7, curr_s7, t, flipx, flipy);
}

uint32_t SpriteInstance::sa_count() const { return static_cast<uint32_t>(data->tracks[track_idx].runs.size()); }

std::vector<SpriteInstance::RunState> SpriteInstance::run_states() const {
	std::vector<RunState> out;
	if (!data || track_idx >= data->tracks.size())
		return out;
	const auto& track = data->tracks[track_idx];
	out.reserve(track.runs.size());
	for (uint32_t i = 0; i < track.runs.size() && i < offsets.size(); ++i) {
		const auto& run = track.runs[i];
		RunState state;
		state.offset = offsets[i];
		state.done = i < done.size() && done[i] != 0;
		state.hidden = i < hide.size() && hide[i] != 0;
		if (run.s8_count > 0 && run.s8_start + state.offset < data->v77.s8.size()) {
			state.s8 = static_cast<int32_t>(run.s8_start + state.offset);
			const auto& s8 = data->v77.s8[run.s8_start + state.offset];
			state.flags = s8.flags;
			state.frames = s8.s8_duration_ticks;
			state.s6 = s8.s6_keyframe_id;
			state.s7 = s8.s7_transform_id;
		}
		out.push_back(state);
	}
	return out;
}

uint32_t SpriteInstance::frame_counter() const { return _frame_counter; }

glm::vec4 SpriteInstance::track_bounds() const { return data->tracks[track_idx].bounds; }

glm::vec4 SpriteInstance::content_bounds() const {
	float l = INFINITY, t = INFINITY, r = -INFINITY, b = -INFINITY;
	const auto& track = data->tracks[track_idx];
	for (size_t i = 0; i < track.runs.size(); ++i) {
		const auto& run = track.runs[i];
		if (run.s8_count == 0 || i >= offsets.size())
			continue;
		const auto& s8 = data->v77.s8[run.s8_start + offsets[i]];
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
	const auto& track = data->tracks[track_idx];
	for (uint32_t i = 0; i < track.runs.size(); ++i) {
		const auto& run = track.runs[i];
		if (run.s8_count == 0 || i >= offsets.size())
			continue;
		const auto& s8 = data->v77.s8[run.s8_start + offsets[i]];
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
			if ((layer.attributes & ~variant_flags) != 0)
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
