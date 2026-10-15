#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace asb {

// A live scene object the engine would have created (a sprite or character),
// as recorded from the hosted module's create/mutate calls.
struct RuntimeEntity {
	// Which coordinate space the create call targeted. World entities are placed
	// by camera/pos in world units; UI entities live in the fixed 1920-wide
	// design space.
	enum class Space { World, Ui };

	std::string name;
	std::string kind; // originating create call
	std::vector<std::string> aliases;
	std::string mbs;
	std::string ftx;
	std::string track;
	int track_index = -1;
	float pos[3] = {0.0f, 0.0f, 0.0f};
	float scale = 1.0f;
	float dir_x = 1.0f; // facing: -1 mirrored, +1 default
	int alpha = 255;	// 0-255, from DrUiSetColorA (0 = not yet faded in)
	int layer = -1;
	bool visible = true;
	int order = 0;
	Space space = Space::World;
	std::string parent; // SetSpriteToAnchor target (empty = none); pos is relative
};

struct RuntimeCamera {
	float pos[3] = {0.0f, 0.0f, 0.0f};
	bool has_pos = false;
	float zoom = 0.0f;
	bool has_zoom = false;
	float accel = 0.0f;
	float decel = 0.0f;
};

// A whole-screen colour fade (CrossFadeAsync/ColorFadeAsync). The overlay
// colour is lerped from `from` to `to` (linear RGBA, 0..1) over [start, end].
struct ScreenFade {
	long start = 0;
	long end = 0;
	float from[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float to[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

// DrCreateStage(stageType, weatherType, coaPreset, flagColorIndex).
struct StageConfig {
	long frame = 0;
	int stage_type = 0;
	int weather_type = 0;
	int coa_preset = 0;
	int flag_color = 0;
};

// The whole scene at one frame. Only emitted when something changed.
struct Keyframe {
	long frame = 0;
	std::vector<RuntimeEntity> entities;
	RuntimeCamera camera;
};

struct Timeline {
	std::string source;
	std::string function;
	std::vector<Keyframe> keyframes;
	std::vector<ScreenFade> fades;
	std::vector<StageConfig> stages;
};

// Overlay colour (linear RGBA) produced by the fades list at `frame`.
inline void fade_at(const std::vector<ScreenFade>& fades, long frame, float* out) {
	out[0] = out[1] = out[2] = out[3] = 0.0f;
	for (const auto& s : fades) {
		if (frame >= s.end) {
			for (int k = 0; k < 4; ++k)
				out[k] = s.to[k];
		} else if (frame >= s.start) {
			const float t = static_cast<float>(frame - s.start) / static_cast<float>(std::max(1L, s.end - s.start));
			for (int k = 0; k < 4; ++k)
				out[k] = s.from[k] + (s.to[k] - s.from[k]) * t;
		}
	}
}

} // namespace asb
