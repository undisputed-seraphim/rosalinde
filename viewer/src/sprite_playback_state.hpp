#pragma once

#include <impl/mbs/sections.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

struct SpriteData;

// Viewer-only debugging knobs, read once at startup instead of from deep
// playback code. Unset values keep the production behavior.
struct SpritePlaybackOptions {
	bool main_only = false;
	int run_only = -1;
	int run_from = -1;
	int source_filter = -1;
};

const SpritePlaybackOptions& sprite_playback_options();

struct SpriteRunState {
	uint32_t offset = 0;
	uint32_t flags = 0;
	uint32_t frames = 0;
	int32_t s8 = -1;
	uint32_t s6 = 0;
	uint32_t s7 = 0;
	bool done = false;
	bool hidden = false;
};

// Mutable playhead state for one MBS instance. It owns no drawing data: layer
// geometry and vertices stay in SpriteInstance/renderer code.
struct SpritePlaybackState {
	uint32_t track_idx = 0;
	float accum = 0.0f;
	std::vector<uint32_t> ticks;
	std::vector<uint32_t> offsets;
	std::vector<uint32_t> prev_offsets;
	std::vector<uint32_t> last_visible_offsets;
	std::vector<uint8_t> done; // per-run: a single-pass run has finished
	std::vector<uint8_t> hide; // per-run: a finished run that should stop drawing
	uint32_t frame_counter = 0;
	bool paused = false;
	bool hold_finished = false;

	std::vector<mbs::section_7> transition_src_s7;
	float transition_t = 1.0f;

	void play(const SpriteData& data, uint32_t track_id, const SpritePlaybackOptions& options);
	void play(const SpriteData& data, const std::string& name, const SpritePlaybackOptions& options);
	void advance(const SpriteData& data, float dt_seconds);
	void update(const SpriteData& data, float dt_seconds);
	void next_track(const SpriteData& data, const SpritePlaybackOptions& options);
	void prev_track(const SpriteData& data, const SpritePlaybackOptions& options);

	uint32_t sa_count(const SpriteData& data) const;
	uint32_t frame_counter_value() const { return frame_counter; }
	std::vector<SpriteRunState> run_states(const SpriteData& data) const;
	glm::mat4 transform_for_sa(const SpriteData& data, uint32_t sa_idx) const;
};
