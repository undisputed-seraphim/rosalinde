#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

class ScreenFadeOverlay {
public:
	ScreenFadeOverlay();
	~ScreenFadeOverlay();
	ScreenFadeOverlay(const ScreenFadeOverlay&) = delete;
	ScreenFadeOverlay& operator=(const ScreenFadeOverlay&) = delete;

	void draw(const glm::vec4& rgba);

private:
	GLuint _vao = 0;
	GLuint _vbo = 0;
	GLuint _program = 0;
	GLint _u_color = -1;
};
