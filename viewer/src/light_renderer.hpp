#pragma once

#include "camera.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

struct PointLightDraw {
	glm::vec3 pos{0.0f};
	glm::vec4 light{0.0f};
	float power = 500.0f;
};

// Additive screen-space point-light pass. A first approximation of the engine's
// lighting: each light is a smooth radial contribution in design/UV space.
// TODO(lighting): shadow/god-ray/bloom and a real accumulated lighting pass.
class LightRenderer {
public:
	LightRenderer();
	~LightRenderer();
	LightRenderer(const LightRenderer&) = delete;
	LightRenderer& operator=(const LightRenderer&) = delete;

	void draw(const glm::mat4& projection, const Camera& cam, const std::vector<PointLightDraw>& lights);

private:
	GLuint _vao = 0;
	GLuint _vbo = 0;
	GLuint _program = 0;
	GLint _u_light = -1;
	GLint _u_color = -1;
	GLint _u_count = -1;
};
