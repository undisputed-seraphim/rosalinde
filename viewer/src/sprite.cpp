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
// How a finished single-pass (s8_sum_once < 0) sequence behaves:
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
	for (uint32_t s6_id = 0; s6_id < v77.s6.size(); ++s6_id) {
		const auto& s6 = v77.s6[s6_id];
		auto& ck = keyframes[s6_id];
		ck.bounds = {s6.left, s6.top, s6.right, s6.bottom};

		ck.layers.reserve(s6.s4_no);
		for (uint32_t j = 0; j < s6.s4_no; ++j) {
			const auto& s4 = v77.s4[s6.s4_id + j];
			if (s4.tex_id >= textures.size())
				continue;

			const auto& tex = textures[s4.tex_id];
			const auto texdim = glm::vec2{tex.width, tex.height};

			CachedKeyframe::Layer layer;
			layer.tex_id = s4.tex_id;
			layer.attributes = s4.attributes;
			layer.blend = s4.blend_id;
			for (int k = 0; k < 4; ++k) {
				layer.uv[k] = v77.s1[s4.s1_id].values[k] * texdim;
				layer.xy[k] = v77.s2[s4.s2_id].values[k];
				layer.color[k] = v77.s0[s4.s0_id].colors[k];
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
		track.sa_set_main = s9.sa_set_main;
		track.disabled = s9.disabled;
		track.runs.reserve(s9.sa_set_no);
		for (uint32_t i = 0; i < s9.sa_set_no; ++i) {
			const auto& sa = v77.sa[s9.sa_set_id + i];
			uint32_t loop_start = 0;
			if (sa.s8_sum_once > 0) {
				uint32_t acc = 0;
				for (uint32_t j = 0; j < sa.s8_no; ++j) {
					acc += v77.s8[sa.s8_id + j].frames;
					if (acc >= (uint32_t)sa.s8_sum_once) {
						loop_start = j + 1;
						break;
					}
				}
			}
			// s8_sum_once < 0 == single-pass ("transitional"/background); play
			// through once and hold the final frame instead of looping.
			track.runs.push_back(
				{sa.s8_id, sa.s8_no, sa.s8_st, loop_start, sa.s8_sum_once < 0 && once_mode() != 2, sa.track_id});
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
			_transition_src_s7[i] = data->v77.s7[s8.s7_id];
		}
		_transition_t = 0.0f;
	}

	track_idx = track_id;
	const auto& track = data->tracks[track_idx];
	ticks.assign(track.runs.size(), 0);
	offsets.resize(track.runs.size());
	prev_offsets.assign(track.runs.size(), UINT32_MAX);
	done.assign(track.runs.size(), 0);
	hide.assign(track.runs.size(), 0);
	if (std::getenv("VIEWER_MAIN_ONLY")) {
		for (uint32_t i = 0; i < hide.size(); ++i)
			hide[i] = i != track.sa_set_main;
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
		_phase = std::min<uint32_t>(track.sa_set_main, track.runs.size() ? track.runs.size() - 1 : 0);
	_accum = 0.0f;

	for (uint32_t i = 0; i < track.runs.size(); ++i) {
		const auto& run = track.runs[i];
		offsets[i] = run.s8_st;
		if (run.s8_count > 0 && run.s8_start < data->v77.s8.size()) {
			ticks[i] = data->v77.s8[run.s8_start + offsets[i]].frames;
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
	constexpr float kTicksPerSecond = 60.0f;
	constexpr float kTransitionDuration = 0.15f;
	_accum += dt_seconds * kTicksPerSecond;

	if (_transition_t < 1.0f) {
		_transition_t += dt_seconds / kTransitionDuration;
		if (_transition_t > 1.0f)
			_transition_t = 1.0f;
	}

	bool advanced = false;
	const auto& track = data->tracks[track_idx];

	while (_accum >= 1.0f) {
		_accum -= 1.0f;
		const uint32_t first = seq_runs() ? std::min<uint32_t>(_phase, track.runs.size()) : 0;
		const uint32_t last = seq_runs() ? std::min<uint32_t>(_phase + 1, track.runs.size()) : track.runs.size();
		for (uint32_t i = first; i < last; ++i) {
			const auto& run = track.runs[i];
			if (run.s8_count == 0 || (i < done.size() && done[i]))
				continue;
			if (--ticks[i] == 0) {
				advanced = true;
				const auto& prev_s8 = data->v77.s8[run.s8_start + offsets[i]];
				prev_offsets[i] = offsets[i];
				if (prev_s8.flags & mbs::v77::s8flag::JUMP) {
					if (prev_s8.loop_s8_id != 0) {
						offsets[i] = (offsets[i] + prev_s8.loop_s8_id) % run.s8_count;
					} else if (prev_s8.flags & mbs::v77::s8flag::LAST) {
						if (run.once) {
							done[i] = 1;
							offsets[i] = run.s8_count - 1;
						} else {
							offsets[i] = 0;
						}
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
				// Other single-pass parts hide when the last frame is an end marker.
				if (done[i])
					hide[i] = hold_finished
								  ? 0
								  : ((data->v77.s8[run.s8_start + offsets[i]].flags & mbs::v77::s8flag::LAST) ? 1 : 0);
				ticks[i] = data->v77.s8[run.s8_start + offsets[i]].frames;
			}
		}
		// Sequential-runs mode: once a phase finishes, start the next one.
		if (seq_runs() && _phase < track.runs.size() && done[_phase] && _phase + 1 < track.runs.size()) {
			++_phase;
			const auto& r = track.runs[_phase];
			offsets[_phase] = r.s8_st;
			if (r.s8_count && r.s8_start < data->v77.s8.size())
				ticks[_phase] = data->v77.s8[r.s8_start + offsets[_phase]].frames;
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

	const auto& track = data->tracks[track_idx];
	if (sa_idx >= track.runs.size())
		return;

	const auto& run = track.runs[sa_idx];
	if (run.s8_count == 0)
		return;
	if (sa_idx < hide.size() && hide[sa_idx])
		return;
	if (seq_runs() && sa_idx != _phase)
		return;

	const auto& s8 = data->v77.s8[run.s8_start + offsets[sa_idx]];
	// Draw only frames flagged ACTIVE (0x20). 0x400 is a common modifier bit
	// that co-occurs with ACTIVE on normal frames (not a "hitbox-only" marker),
	// and 0x800 without ACTIVE is a non-rendered end sentinel — both covered by
	// the ACTIVE test.
	if (!(s8.flags & mbs::v77::s8flag::ACTIVE))
		return;

	const auto& ck = data->keyframes[s8.s6_id];
	if (ck.layers.empty())
		return;

	uint32_t visible = 0;
	for (const auto& layer : ck.layers) {
		if ((layer.attributes & ~variant_flags) == 0 && (layer.attributes & ~layer_mask) == 0)
			visible++;
	}
	if (visible == 0)
		return;

	const float zrate = 1.0f / (visible + 1.0f);
	float depth = 1.0f;
	uint32_t base = 0;

	for (const auto& layer : ck.layers) {
		if ((layer.attributes & ~variant_flags) != 0 || (layer.attributes & ~layer_mask) != 0)
			continue;
		if (blend_filter >= 0 && layer.blend != blend_filter)
			continue;

		uint32_t c0 = layer.color[0], c1 = layer.color[1], c2 = layer.color[2], c3 = layer.color[3];
		if (tints) {
			auto it = tints->find(layer.attributes);
			if (it != tints->end()) {
				const auto& t = it->second;
				auto mul = [](uint32_t c, const glm::vec4& t) {
					uint32_t r = ((c >> 0) & 0xFF) * t.r;
					uint32_t g = ((c >> 8) & 0xFF) * t.g;
					uint32_t b = ((c >> 16) & 0xFF) * t.b;
					uint32_t a = ((c >> 24) & 0xFF) * t.a;
					return (a << 24) | (b << 16) | (g << 8) | r;
				};
				c0 = mul(c0, t);
				c1 = mul(c1, t);
				c2 = mul(c2, t);
				c3 = mul(c3, t);
			}
		}

		if (apply_fog) {
			const uint32_t fog = data->v77.s7[s8.s7_id].fog;
			const glm::vec4 f{
				((fog >> 16) & 0xFF) / 255.0f,
				((fog >> 8) & 0xFF) / 255.0f,
				(fog & 0xFF) / 255.0f,
				((fog >> 24) & 0xFF) / 255.0f};
			auto fmul = [](uint32_t c, const glm::vec4& f) {
				uint32_t r = ((c >> 0) & 0xFF) * f.r;
				uint32_t g = ((c >> 8) & 0xFF) * f.g;
				uint32_t b = ((c >> 16) & 0xFF) * f.b;
				uint32_t a = ((c >> 24) & 0xFF) * f.a;
				return (a << 24) | (b << 16) | (g << 8) | r;
			};
			c0 = fmul(c0, f);
			c1 = fmul(c1, f);
			c2 = fmul(c2, f);
			c3 = fmul(c3, f);
		}

		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[0], {layer.xy[0], depth}, c0});
		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[1], {layer.xy[1], depth}, c1});
		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[2], {layer.xy[2], depth}, c2});
		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[3], {layer.xy[3], depth}, c3});

		indices.insert(indices.end(), {base, base + 1, base + 3, base + 1, base + 2, base + 3});
		depth -= zrate;
		base += 4;
	}
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

	const auto& curr_s7 = data->v77.s7[s8.s7_id];

	if (_transition_t < 1.0f && !_transition_src_s7.empty()) {
		uint32_t src_idx = sa_idx < _transition_src_s7.size() ? sa_idx : (_transition_src_s7.size() - 1);
		float t = glm::smoothstep(0.0f, 1.0f, _transition_t);
		return s7_matrix_interp(_transition_src_s7[src_idx], curr_s7, t, flipx, flipy);
	}

	if (s8.s7_interpolation == 0 || prev_offsets[sa_idx] == UINT32_MAX) {
		return mbs::s7_matrix(curr_s7, flipx, flipy);
	}

	const auto& prev_s8 = data->v77.s8[run.s8_start + prev_offsets[sa_idx]];
	const auto& prev_s7 = data->v77.s7[prev_s8.s7_id];
	float t = 1.0f - (float)ticks[sa_idx] / (float)s8.frames;
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
			const auto& s8 = data->v77.s8[run.s8_start + state.offset];
			state.flags = s8.flags;
			state.frames = s8.frames;
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
		const glm::mat4 m = mbs::s7_matrix(data->v77.s7[s8.s7_id], false, false);
		const glm::vec4 kb = data->keyframes[s8.s6_id].bounds;
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
		if (s8.flags & mbs::v77::s8flag::HITBOX)
			continue;
		if ((s8.flags & mbs::v77::s8flag::LAST) && !(s8.flags & mbs::v77::s8flag::ACTIVE))
			continue;
		// Skip heavily-scaled runs (light shafts, insert flashes): they are
		// overlays and would otherwise blow up the framing bounds.
		const auto& s7 = data->v77.s7[s8.s7_id];
		if (std::max(std::abs(s7.scale.x), std::abs(s7.scale.y)) > 4.0f)
			continue;
		const auto& ck = data->keyframes[s8.s6_id];
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
