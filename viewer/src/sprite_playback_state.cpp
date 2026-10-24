#include "sprite_playback_state.hpp"

#include "sprite.hpp"

#include <glm/gtx/euler_angles.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace mbs {
glm::mat4 s7_matrix(const section_7& s7, bool flipx, bool flipy);
}

namespace {

glm::mat4 s7_matrix_interp(const mbs::section_7& a, const mbs::section_7& b, float t, bool flipx, bool flipy) {
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

SpritePlaybackOptions read_options() {
	SpritePlaybackOptions options;
	options.main_only = std::getenv("VIEWER_MAIN_ONLY") != nullptr;
	if (const char* value = std::getenv("VIEWER_RUN_ONLY"))
		options.run_only = std::atoi(value);
	if (const char* value = std::getenv("VIEWER_RUN_FROM"))
		options.run_from = std::atoi(value);
	if (const char* value = std::getenv("VIEWER_S4_SOURCE"))
		options.source_filter = std::atoi(value);
	return options;
}

bool can_hold(const SpriteData& data, const SpriteData::Track& track, uint32_t run_idx, uint32_t offset) {
	const auto& run = track.runs[run_idx];
	if (run.s8_start + offset >= data.v77.s8.size())
		return false;
	const auto& s8 = data.v77.s8[run.s8_start + offset];
	if (s8.s6_keyframe_id >= data.keyframes.size())
		return false;
	const auto& keyframe = data.keyframes[s8.s6_keyframe_id];
	const float width = std::abs(keyframe.bounds.z - keyframe.bounds.x);
	const float height = std::abs(keyframe.bounds.w - keyframe.bounds.y);
	if (width > 3000.0f || height > 3000.0f)
		return false;
	bool textured = false;
	for (const auto& layer : keyframe.layers) {
		if (!layer.notex)
			textured = true;
	}
	return textured;
}

} // namespace

const SpritePlaybackOptions& sprite_playback_options() {
	static const SpritePlaybackOptions options = read_options();
	return options;
}

void SpritePlaybackState::play(const SpriteData& data, uint32_t track_id, const SpritePlaybackOptions& options) {
	if (track_id >= data.tracks.size())
		return;
	if (track_idx < data.tracks.size() && track_idx != track_id &&
		offsets.size() == data.tracks[track_idx].runs.size()) {
		const uint32_t count = static_cast<uint32_t>(data.tracks[track_idx].runs.size());
		transition_src_s7.resize(count);
		for (uint32_t i = 0; i < count; ++i) {
			const auto& run = data.tracks[track_idx].runs[i];
			if (run.s8_count == 0)
				continue;
			const auto& s8 = data.v77.s8[run.s8_start + offsets[i]];
			transition_src_s7[i] = data.v77.s7[s8.s7_transform_id];
		}
		transition_t = 0.0f;
	}

	track_idx = track_id;
	const auto& track = data.tracks[track_idx];
	ticks.assign(track.runs.size(), 0);
	offsets.resize(track.runs.size());
	prev_offsets.assign(track.runs.size(), UINT32_MAX);
	last_visible_offsets.assign(track.runs.size(), UINT32_MAX);
	done.assign(track.runs.size(), 0);
	hide.assign(track.runs.size(), 0);
	if (options.main_only) {
		for (uint32_t i = 0; i < hide.size(); ++i)
			hide[i] = i != track.sa_main_sequence;
	}
	if (options.run_only >= 0) {
		for (uint32_t i = 0; i < hide.size(); ++i)
			hide[i] = static_cast<int>(i) != options.run_only;
	}
	if (options.run_from >= 0) {
		for (uint32_t i = 0; i < hide.size(); ++i)
			hide[i] = static_cast<int>(i) < options.run_from;
	}
	accum = 0.0f;

	for (uint32_t i = 0; i < track.runs.size(); ++i) {
		const auto& run = track.runs[i];
		offsets[i] = run.s8_start_offset;
		if (run.s8_count > 0 && run.s8_start < data.v77.s8.size())
			ticks[i] = data.v77.s8[run.s8_start + offsets[i]].s8_duration_ticks;
	}
	frame_counter++;
	spdlog::debug("anim: play track {} \"{}\"", track_id, track.name);
}

void SpritePlaybackState::play(const SpriteData& data, const std::string& name, const SpritePlaybackOptions& options) {
	for (uint32_t i = 0; i < data.tracks.size(); ++i) {
		if (data.tracks[i].name == name) {
			play(data, i, options);
			return;
		}
	}
}

void SpritePlaybackState::update(const SpriteData& data, float dt_seconds) {
	// MBS animation sequences advance at 120 frames/s (script Wait/time units are
	// 60 Hz). Confirmed against the opening reference: a 556-frame logo sequence
	// ends at 4.6 s, and the title's flash phase lands at 14 s.
	constexpr float kTicksPerSecond = 120.0f;
	constexpr float kTransitionDuration = 0.15f;
	accum += dt_seconds * kTicksPerSecond;

	if (transition_t < 1.0f) {
		transition_t += dt_seconds / kTransitionDuration;
		if (transition_t > 1.0f)
			transition_t = 1.0f;
	}

	bool advanced = false;
	const auto& track = data.tracks[track_idx];
	while (accum >= 1.0f) {
		accum -= 1.0f;
		for (uint32_t i = 0; i < track.runs.size(); ++i) {
			const auto& run = track.runs[i];
			if (run.s8_count == 0 || (i < done.size() && done[i]) || offsets[i] >= run.s8_count)
				continue;
			const auto& current_s8 = data.v77.s8[run.s8_start + offsets[i]];
			if ((current_s8.flags & mbs::v77::s8flag::ACTIVE) &&
				((data.v77.s7[current_s8.s7_transform_id].fog >> 24) != 0))
				last_visible_offsets[i] = offsets[i];
			if (--ticks[i] == 0) {
				advanced = true;
				const auto& prev_s8 = current_s8;
				prev_offsets[i] = offsets[i];
				if (track.one_shot && offsets[i] + 1 >= run.s8_count) {
					done[i] = 1;
					offsets[i] = run.s8_count - 1;
				} else if (prev_s8.flags & mbs::v77::s8flag::JUMP) {
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
					const auto& end = data.v77.s8[run.s8_start + offsets[i]];
					const uint32_t fog = data.v77.s7[end.s7_transform_id].fog;
					const bool visible_end = (end.flags & mbs::v77::s8flag::ACTIVE) && ((fog >> 24) != 0);
					const bool one_tick_last = run.once && !hold_finished && end.s8_duration_ticks <= 1 &&
											   (end.flags & mbs::v77::s8flag::LAST);
					if (one_tick_last && track.runs.size() > 1 && last_visible_offsets[i] != UINT32_MAX &&
						can_hold(data, track, i, last_visible_offsets[i])) {
						offsets[i] = last_visible_offsets[i];
						hide[i] = 0;
					} else if (one_tick_last && prev_offsets[i] != UINT32_MAX) {
						const auto& prev = data.v77.s8[run.s8_start + prev_offsets[i]];
						const uint32_t prev_fog = data.v77.s7[prev.s7_transform_id].fog;
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
				ticks[i] = data.v77.s8[run.s8_start + offsets[i]].s8_duration_ticks;
			}
		}
	}

	if (advanced)
		frame_counter++;
}

void SpritePlaybackState::advance(const SpriteData& data, float dt_seconds) {
	if (paused)
		return;
	update(data, dt_seconds);
}

void SpritePlaybackState::next_track(const SpriteData& data, const SpritePlaybackOptions& options) {
	if (data.tracks.empty())
		return;
	play(data, static_cast<uint32_t>((track_idx + 1) % data.tracks.size()), options);
}

void SpritePlaybackState::prev_track(const SpriteData& data, const SpritePlaybackOptions& options) {
	if (data.tracks.empty())
		return;
	play(data, static_cast<uint32_t>(track_idx == 0 ? data.tracks.size() - 1 : track_idx - 1), options);
}

uint32_t SpritePlaybackState::sa_count(const SpriteData& data) const {
	if (track_idx >= data.tracks.size())
		return 0;
	return static_cast<uint32_t>(data.tracks[track_idx].runs.size());
}

std::vector<SpriteRunState> SpritePlaybackState::run_states(const SpriteData& data) const {
	std::vector<SpriteRunState> out;
	if (track_idx >= data.tracks.size())
		return out;
	const auto& track = data.tracks[track_idx];
	out.reserve(track.runs.size());
	for (uint32_t i = 0; i < track.runs.size() && i < offsets.size(); ++i) {
		const auto& run = track.runs[i];
		SpriteRunState state;
		state.offset = offsets[i];
		state.done = i < done.size() && done[i] != 0;
		state.hidden = i < hide.size() && hide[i] != 0;
		if (run.s8_count > 0 && run.s8_start + state.offset < data.v77.s8.size()) {
			state.s8 = static_cast<int32_t>(run.s8_start + state.offset);
			const auto& s8 = data.v77.s8[run.s8_start + state.offset];
			state.flags = s8.flags;
			state.frames = s8.s8_duration_ticks;
			state.s6 = s8.s6_keyframe_id;
			state.s7 = s8.s7_transform_id;
		}
		out.push_back(state);
	}
	return out;
}

glm::mat4 SpritePlaybackState::transform_for_sa(const SpriteData& data, uint32_t sa_idx) const {
	if (track_idx >= data.tracks.size())
		return glm::mat4{1.0f};
	const auto& track = data.tracks[track_idx];
	if (sa_idx >= track.runs.size() || sa_idx >= offsets.size())
		return glm::mat4{1.0f};
	const auto& run = track.runs[sa_idx];
	const auto& s8 = data.v77.s8[run.s8_start + offsets[sa_idx]];

	const bool flipx = s8.flags & mbs::v77::s8flag::FLIPX;
	const bool flipy = s8.flags & mbs::v77::s8flag::FLIPY;
	const auto& current_s7 = data.v77.s7[s8.s7_transform_id];

	if (transition_t < 1.0f && !transition_src_s7.empty()) {
		const uint32_t source_idx = sa_idx < transition_src_s7.size() ? sa_idx : (transition_src_s7.size() - 1);
		const float t = glm::smoothstep(0.0f, 1.0f, transition_t);
		return s7_matrix_interp(transition_src_s7[source_idx], current_s7, t, flipx, flipy);
	}

	if (s8.s7_transform_interpolation == 0 || prev_offsets[sa_idx] == UINT32_MAX)
		return mbs::s7_matrix(current_s7, flipx, flipy);

	const auto& prev_s8 = data.v77.s8[run.s8_start + prev_offsets[sa_idx]];
	const auto& prev_s7 = data.v77.s7[prev_s8.s7_transform_id];
	const float t = 1.0f - static_cast<float>(ticks[sa_idx]) / static_cast<float>(s8.s8_duration_ticks);
	return s7_matrix_interp(prev_s7, current_s7, t, flipx, flipy);
}
