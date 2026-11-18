#pragma once

#include "weather.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>

// Screen-space precipitation fallback. BGEffect.mbs authors fog and splash
// sprites but not falling streaks, so the viewer generates one instanced
// particle field for the whole view. The field carries the preset intensity
// and viewer tuning knobs; only rain is implemented, snow is still unknown.
class WeatherRenderer {
public:
	WeatherRenderer();
	~WeatherRenderer();
	WeatherRenderer(const WeatherRenderer&) = delete;
	WeatherRenderer& operator=(const WeatherRenderer&) = delete;

	void draw(const ScreenField& field, float request, float time);

private:
	GLuint _vao = 0;
	GLuint _vbo = 0;
};
