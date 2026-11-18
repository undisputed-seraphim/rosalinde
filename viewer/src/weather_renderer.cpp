#include "weather_renderer.hpp"
#include "scene_util.hpp"
#include "shader.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

WeatherRenderer::WeatherRenderer() {
	static const float quad[] = {
		0.0f,
		0.0f,
		1.0f,
		0.0f,
		1.0f,
		1.0f,
		0.0f,
		0.0f,
		1.0f,
		1.0f,
		0.0f,
		1.0f,
	};
	glGenVertexArrays(1, &_vao);
	glGenBuffers(1, &_vbo);
	glBindVertexArray(_vao);
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

WeatherRenderer::~WeatherRenderer() {
	if (_vbo)
		glDeleteBuffers(1, &_vbo);
	if (_vao)
		glDeleteVertexArrays(1, &_vao);
}

void WeatherRenderer::draw(const glm::vec2& ndc_min, const glm::vec2& ndc_max, float amount, float time) {
	static constexpr int max_streaks = 10000;
	const bool depth = glIsEnabled(GL_DEPTH_TEST);
	glDisable(GL_DEPTH_TEST);
	enable_blend(glm::vec4(1.0f));

	const auto& shader = GetWeatherShader().Use();
	shader.SetUniform("u_ndc_min", ndc_min.x, ndc_min.y);
	shader.SetUniform("u_ndc_max", ndc_max.x, ndc_max.y);
	shader.SetUniform("u_amount", amount);
	shader.SetUniform("u_time", time);

	const int streaks = std::clamp(static_cast<int>(std::lround(amount * max_streaks)), 0, max_streaks);
	glBindVertexArray(_vao);
	glDrawArraysInstanced(GL_TRIANGLES, 0, 6, streaks);
	glBindVertexArray(0);

	if (depth)
		glEnable(GL_DEPTH_TEST);
}
