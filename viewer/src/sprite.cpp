#include "sprite.hpp"

#include <criware/byte_reader.hpp>
#include <impl/mbs/detail.hpp>

#include <cstring>
#include <span>
#include <stdexcept>

namespace mbs {
	glm::mat4 s7_matrix(const section_7& s7, bool flipx, bool flipy);
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
			if (s4.tex_id >= textures.size()) continue;

			const auto& tex = textures[s4.tex_id];
			const auto texdim = glm::vec2{tex.width, tex.height};

			CachedKeyframe::Layer layer;
			layer.tex_id = s4.tex_id;
			layer.attributes = s4.attributes;
			for (int k = 0; k < 4; ++k) {
				layer.uv[k] = v77.s1[s4.s1_id].values[k] * texdim;
				layer.xy[k] = v77.s2[s4.s2_id].values[k];
				layer.color[k] = v77.s0[s4.s0_id].colors[k];
			}
			ck.layers.push_back(layer);
		}
	}

	tracks.resize(v77.s9.size());
	for (uint32_t s9_id = 0; s9_id < v77.s9.size(); ++s9_id) {
		const auto& s9 = v77.s9[s9_id];
		auto& track = tracks[s9_id];
		track.name = s9.name;
		track.name = track.name.c_str();
		track.bounds = {s9.left, s9.top, s9.right, s9.bottom};
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
			track.runs.push_back({sa.s8_id, sa.s8_no, sa.s8_st, loop_start});
		}
	}
}

const SpriteData::Track* SpriteData::find_track(const std::string& name) const {
	for (const auto& t : tracks) {
		if (t.name == name) return &t;
	}
	return nullptr;
}

void SpriteInstance::play(uint32_t track_id) {
	track_idx = track_id;
	const auto& track = data->tracks[track_idx];
	ticks.assign(track.runs.size(), 0);
	offsets.resize(track.runs.size());
	_accum = 0.0f;

	for (uint32_t i = 0; i < track.runs.size(); ++i) {
		const auto& run = track.runs[i];
		offsets[i] = run.s8_st;
		if (run.s8_count > 0 && run.s8_start < data->v77.s8.size()) {
			ticks[i] = data->v77.s8[run.s8_start + offsets[i]].frames;
		}
	}
	_frame_counter++;
	fprintf(stdout, "%u: %s\n", track_id, track.name.c_str());
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
	_accum += dt_seconds * kTicksPerSecond;

	bool advanced = false;
	const auto& track = data->tracks[track_idx];

	while (_accum >= 1.0f) {
		_accum -= 1.0f;
		for (uint32_t i = 0; i < track.runs.size(); ++i) {
			const auto& run = track.runs[i];
			if (run.s8_count == 0) continue;
			if (--ticks[i] == 0) {
				advanced = true;
				const auto& prev_s8 = data->v77.s8[run.s8_start + offsets[i]];
				if (prev_s8.flags & mbs::v77::s8flag::JUMP) {
					offsets[i] = (offsets[i] + prev_s8.loop_s8_id) % run.s8_count;
				} else {
					++offsets[i];
					if (offsets[i] >= run.s8_count) offsets[i] = run.loop_start;
				}
				ticks[i] = data->v77.s8[run.s8_start + offsets[i]].frames;
			}
		}
	}

	if (advanced) _frame_counter++;
}

void SpriteInstance::next_track() {
	if (data->tracks.empty()) return;
	play((track_idx + 1) % data->tracks.size());
}

void SpriteInstance::prev_track() {
	if (data->tracks.empty()) return;
	play(track_idx == 0 ? data->tracks.size() - 1 : track_idx - 1);
}

void SpriteInstance::build_vertices(
	uint32_t sa_idx, std::vector<SpriteVertex>& verts, std::vector<uint32_t>& indices) const {
	verts.clear();
	indices.clear();

	const auto& track = data->tracks[track_idx];
	if (sa_idx >= track.runs.size()) return;

	const auto& run = track.runs[sa_idx];
	if (run.s8_count == 0) return;

	const auto& s8 = data->v77.s8[run.s8_start + offsets[sa_idx]];
	if (s8.flags & (mbs::v77::s8flag::HITBOX | mbs::v77::s8flag::LAST)) return;

	const auto& ck = data->keyframes[s8.s6_id];
	if (ck.layers.empty()) return;

	uint32_t visible = 0;
	for (const auto& layer : ck.layers) {
		if ((layer.attributes & ~variant_flags) == 0) visible++;
	}
	if (visible == 0) return;

	const float zrate = 1.0f / (visible + 1.0f);
	float depth = 1.0f;
	uint32_t base = 0;

	for (const auto& layer : ck.layers) {
		if ((layer.attributes & ~variant_flags) != 0) continue;

		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[0], {layer.xy[0], depth}, layer.color[0]});
		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[1], {layer.xy[1], depth}, layer.color[1]});
		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[2], {layer.xy[2], depth}, layer.color[2]});
		verts.push_back({static_cast<int16_t>(layer.tex_id), layer.uv[3], {layer.xy[3], depth}, layer.color[3]});

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
	if (out_flipx) *out_flipx = flipx;
	if (out_flipy) *out_flipy = flipy;

	return mbs::s7_matrix(data->v77.s7[s8.s7_id], flipx, flipy);
}

uint32_t SpriteInstance::sa_count() const {
	return static_cast<uint32_t>(data->tracks[track_idx].runs.size());
}

uint32_t SpriteInstance::frame_counter() const {
	return _frame_counter;
}

glm::vec4 SpriteInstance::track_bounds() const {
	return data->tracks[track_idx].bounds;
}
