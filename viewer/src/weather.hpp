#pragma once

#include "stage_asset.hpp"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

// Drama weather is a preset-gated overlay. The BBS object list places the
// layers, the stage's atmosphere preset selects them, and each layer's MBS
// track decides how it is drawn. Precipitation that is not authored as a
// sprite layer (falling streaks) becomes a screen field instead.
enum class OverlayKind {
	Sprite,
	Splash,
	RainField,
	SnowField,
};

struct WeatherPreset {
	int index = 0;
	float cloud = 0.0f;
	float fog = 0.0f;
};

// One synthetic screen-space pass. The BBS fog quad is only used to test
// whether the camera sees the weather; the pass itself is drawn over the
// whole view. Parameters are viewer tuning knobs, not engine constants.
struct ScreenField {
	OverlayKind kind = OverlayKind::Sprite;
	float cloud = 0.0f;
	float density = 1.0f;
	float speed = 1.0f;
	float width = 1.0f;
	float length = 1.0f;
	float tilt = 1.0f;
	std::array<glm::vec2, 4> corners{};
};

struct OverlayLayer {
	StageLayer spec;
	OverlayKind kind = OverlayKind::Sprite;
	size_t index = 0;
};

// Track data the composer needs; the caller owns the asset cache.
struct OverlayTrack {
	std::string_view name;
	glm::vec4 bounds{0.0f};
	bool found = false;
};

struct WeatherFrame {
	WeatherPreset preset;
	std::vector<OverlayLayer> layers;
	std::vector<OverlayLayer> hidden;
	std::vector<ScreenField> fields;
};

OverlayKind classify_overlay(std::string_view track_name);

std::string_view overlay_kind_name(OverlayKind kind);

// BBS objects carry one bit per atmosphere preset. Zero means un-gated.
bool preset_selects(uint8_t mask, int preset_index);

WeatherFrame compose_weather(
	const std::vector<StageLayer>& layers,
	const WeatherPreset& preset,
	const std::function<OverlayTrack(const StageLayer&)>& track);
