#include "light_renderer.hpp"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace {

constexpr int kMaxLights = 32;

constexpr const char* k_vert = "#version 450\n"
							   "layout(location = 0) in vec2 a_pos;\n"
							   "out vec2 v_uv;\n"
							   "void main() { v_uv = a_pos * 0.5 + 0.5; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

constexpr const char* k_frag = "#version 450\n"
							   "in vec2 v_uv;\n"
							   "uniform vec4 u_light[32];\n"
							   "uniform vec4 u_color[32];\n"
							   "uniform int  u_count;\n"
							   "out vec4 fragColor;\n"
							   "void main() {\n"
							   "    vec3 sum = vec3(0.0);\n"
							   "    for (int i = 0; i < u_count; ++i) {\n"
							   "        vec2 d = v_uv - u_light[i].xy;\n"
							   "        float r = u_light[i].z;\n"
							   "        float f = smoothstep(r, 0.0, length(d));\n"
							   "        sum += u_color[i].rgb * u_light[i].w * f;\n"
							   "    }\n"
							   "    fragColor = vec4(sum, 0.0);\n"
							   "}\n";

GLuint compile(GLenum type, const char* src) {
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok = GL_FALSE;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (ok != GL_TRUE) {
		char log[1024] = {};
		glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log);
		spdlog::error("light: shader compile failed: {}", log);
	}
	return s;
}

} // namespace

LightRenderer::LightRenderer() {
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
		spdlog::error("light: program link failed: {}", log);
	}
	_u_light = glGetUniformLocation(_program, "u_light");
	_u_color = glGetUniformLocation(_program, "u_color");
	_u_count = glGetUniformLocation(_program, "u_count");
}

LightRenderer::~LightRenderer() {
	if (_vbo)
		glDeleteBuffers(1, &_vbo);
	if (_vao)
		glDeleteVertexArrays(1, &_vao);
	if (_program)
		glDeleteProgram(_program);
}

void LightRenderer::draw(const glm::mat4& projection, const Camera& cam, const std::vector<PointLightDraw>& lights) {
	if (_program == 0 || lights.empty())
		return;

	glm::vec4 light[kMaxLights];
	glm::vec4 color[kMaxLights];
	int n = 0;
	const glm::mat4 view = cam.lookAt();
	for (const auto& l : lights) {
		if (n >= kMaxLights)
			break;
		const glm::vec4 clip = projection * view * glm::vec4(l.pos, 1.0f);
		if (clip.w <= 1e-4f)
			continue;
		const glm::vec2 ndc = glm::vec2(clip.x / clip.w, clip.y / clip.w);
		const glm::vec2 uv = ndc * 0.5f + 0.5f;

		// `power` is an intensity (default 500), not the radius. Use a bounded
		// world-space radius and a low additive intensity; TODO(lighting): recover
		// the engine's actual light falloff/god-ray model.
		const float radius_world = std::clamp(l.power * 0.4f, 100.0f, 600.0f);
		const glm::vec4 clip2 =
			projection * view * glm::vec4(l.pos + glm::vec3(radius_world, 0.0f, 0.0f), 1.0f);
		if (clip2.w <= 1e-4f)
			continue;
		const glm::vec2 ndc2 = glm::vec2(clip2.x / clip2.w, clip2.y / clip2.w);
		const float radius = glm::length((ndc2 - ndc) * 0.5f);
		if (radius <= 1e-5f)
			continue;

		const float intensity = std::clamp(l.power / 5000.0f, 0.0f, 0.35f);
		light[n] = glm::vec4(uv, radius, intensity);
		color[n] = glm::vec4(l.light.r, l.light.g, l.light.b, 1.0f);
		++n;
	}
	if (n == 0)
		return;

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
	glDisable(GL_DEPTH_TEST);

	glUseProgram(_program);
	glUniform4fv(_u_light, n, &light[0].x);
	glUniform4fv(_u_color, n, &color[0].x);
	glUniform1i(_u_count, n);
	glBindVertexArray(_vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
	glUseProgram(0);
}
