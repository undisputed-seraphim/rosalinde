#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

// Screen-space weather pass. The engine submits rain/snow as procedural streaks
// clipped by a screen-space mask rect per draw; the viewer draws one pass over
// the union of the projected RAIN_FOG/SNOW_FOG regions. The mask is in NDC, so
// callers pass the bounds directly without touching the GL state machine.
class WeatherRenderer {
public:
	WeatherRenderer();
	~WeatherRenderer();
	WeatherRenderer(const WeatherRenderer&) = delete;
	WeatherRenderer& operator=(const WeatherRenderer&) = delete;

	void draw(const glm::vec2& ndc_min, const glm::vec2& ndc_max, float amount, float time);

private:
	GLuint _vao = 0;
	GLuint _vbo = 0;
};
