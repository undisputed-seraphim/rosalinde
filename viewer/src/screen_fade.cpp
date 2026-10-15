#include "screen_fade.hpp"

#include <cstdio>

namespace {

constexpr const char* k_vert = "#version 450\n"
							   "layout(location = 0) in vec2 a_pos;\n"
							   "void main() { gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

constexpr const char* k_frag = "#version 450\n"
							   "uniform vec4 u_color;\n"
							   "out vec4 fragColor;\n"
							   "void main() { fragColor = u_color; }\n";

GLuint compile(GLenum type, const char* src) {
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok = GL_FALSE;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (ok != GL_TRUE) {
		char log[1024] = {};
		glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log);
		std::fprintf(stderr, "[screen_fade] shader compile failed: %s\n", log);
	}
	return s;
}

} // namespace

ScreenFadeOverlay::ScreenFadeOverlay() {
	static const float quad[8] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
	glGenVertexArrays(1, &_vao);
	glGenBuffers(1, &_vbo);
	glBindVertexArray(_vao);
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
	glBindVertexArray(0);

	const GLuint vs = compile(GL_VERTEX_SHADER, k_vert);
	const GLuint fs = compile(GL_FRAGMENT_SHADER, k_frag);
	_program = glCreateProgram();
	glAttachShader(_program, vs);
	glAttachShader(_program, fs);
	glLinkProgram(_program);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint ok = GL_FALSE;
	glGetProgramiv(_program, GL_LINK_STATUS, &ok);
	if (ok != GL_TRUE) {
		char log[1024] = {};
		glGetProgramInfoLog(_program, sizeof(log) - 1, nullptr, log);
		std::fprintf(stderr, "[screen_fade] program link failed: %s\n", log);
	}
	_u_color = glGetUniformLocation(_program, "u_color");
}

ScreenFadeOverlay::~ScreenFadeOverlay() {
	if (_vbo)
		glDeleteBuffers(1, &_vbo);
	if (_vao)
		glDeleteVertexArrays(1, &_vao);
	if (_program)
		glDeleteProgram(_program);
}

void ScreenFadeOverlay::draw(const glm::vec4& rgba) {
	if (_program == 0 || rgba.a <= 0.0f)
		return;
	// The sprite renderer leaves the blend mode stateful (per-sprite MBS blend);
	// force normal alpha for the overlay.
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	glUseProgram(_program);
	glUniform4f(_u_color, rgba.r, rgba.g, rgba.b, rgba.a);
	glBindVertexArray(_vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
	glUseProgram(0);
}
