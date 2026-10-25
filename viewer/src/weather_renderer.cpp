#include "weather_renderer.hpp"
#include "shader.hpp"

#include <glm/glm.hpp>

WeatherRenderer::WeatherRenderer() {
	static const float quad[] = {
		-1.0f,
		-1.0f,
		3.0f,
		-1.0f,
		-1.0f,
		3.0f,
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
	const bool depth = glIsEnabled(GL_DEPTH_TEST);
	const bool blend = glIsEnabled(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	const auto& shader = GetWeatherShader().Use();
	shader.SetUniform("u_ndc_min", ndc_min.x, ndc_min.y);
	shader.SetUniform("u_ndc_max", ndc_max.x, ndc_max.y);
	shader.SetUniform("u_amount", amount);
	shader.SetUniform("u_time", time);

	glBindVertexArray(_vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);

	if (depth)
		glEnable(GL_DEPTH_TEST);
	if (!blend)
		glDisable(GL_BLEND);
}
