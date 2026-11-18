#include "weather.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace {

std::string lower_of(std::string_view text) {
	std::string lower(text);
	std::transform(
		lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

bool weather_prefix(std::string_view lower, std::string_view prefix) {
	if (lower.rfind(prefix, 0) != 0 || lower.size() <= prefix.size())
		return false;
	const char next = lower[prefix.size()];
	return std::isdigit(static_cast<unsigned char>(next)) != 0 || next == '_';
}

} // namespace

OverlayKind classify_overlay(std::string_view track_name) {
	const std::string lower = lower_of(track_name);
	if (lower.rfind("rain_fog", 0) == 0)
		return OverlayKind::RainField;
	if (lower.rfind("snow_fog", 0) == 0)
		return OverlayKind::SnowField;
	if (weather_prefix(lower, "rain") || weather_prefix(lower, "snow"))
		return OverlayKind::Splash;
	return OverlayKind::Sprite;
}

std::string_view overlay_kind_name(OverlayKind kind) {
	switch (kind) {
	case OverlayKind::Sprite:
		return "sprite";
	case OverlayKind::Splash:
		return "splash";
	case OverlayKind::RainField:
		return "rain";
	case OverlayKind::SnowField:
		return "snow";
	}
	return "unknown";
}

bool preset_selects(uint8_t mask, int preset_index) {
	if (mask == 0 || preset_index < 0 || preset_index >= 8)
		return true;
	return (mask & (1u << static_cast<unsigned>(preset_index))) != 0;
}

WeatherFrame compose_weather(
	const std::vector<StageLayer>& layers,
	const WeatherPreset& preset,
	const std::function<OverlayTrack(const StageLayer&)>& track) {
	WeatherFrame frame;
	frame.preset = preset;

	for (size_t i = 0; i < layers.size(); ++i) {
		const auto& spec = layers[i];
		const OverlayTrack info = track(spec);
		if (!info.found)
			continue;

		OverlayLayer layer;
		layer.spec = spec;
		layer.kind = classify_overlay(info.name);
		layer.index = i;

		// Only overlay layers are preset-gated today. Stage layers that the
		// BBS lists for atmospheric effects stay unconditional until the
		// preset mask is extended to every effect track.
		const bool gated = layer.kind != OverlayKind::Sprite;
		if (gated && (!preset_selects(spec.weather_mask, preset.index) || preset.cloud <= 0.01f)) {
			frame.hidden.push_back(std::move(layer));
			continue;
		}

		if (layer.kind == OverlayKind::RainField || layer.kind == OverlayKind::SnowField) {
			ScreenField field;
			field.kind = layer.kind;
			field.cloud = preset.cloud;
			const float sy = spec.scale_y > 0.0f ? spec.scale_y : spec.scale;
			const glm::vec4& b = info.bounds;
			field.corners = {{
				{spec.x + b.x * spec.scale, spec.y + b.y * sy},
				{spec.x + b.z * spec.scale, spec.y + b.y * sy},
				{spec.x + b.z * spec.scale, spec.y + b.w * sy},
				{spec.x + b.x * spec.scale, spec.y + b.w * sy},
			}};
			frame.fields.push_back(field);
		}

		frame.layers.push_back(std::move(layer));
	}
	return frame;
}
