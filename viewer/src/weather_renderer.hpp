#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

// Screen-space weather pass. The engine draws rain as procedural streaks in a
// full-screen quad clipped by the BBS weather-mask rect; this is the viewer's
// port of that pass. The mask is in NDC, so callers can feed the projected
// bounds of the RAIN_FOG/SNOW_FOG quads without touching the GL state machine.
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
