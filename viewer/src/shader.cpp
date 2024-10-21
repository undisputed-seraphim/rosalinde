#include <glad/glad.h>
#include <glm/ext.hpp>
#include <iostream>

#include "shader.hpp"

// clang-format off
constexpr std::string_view drawline_vert_src = "# version 300 es\n"
"in      highp vec2 a_xy;\n"
"uniform highp vec2 u_pxsize;\n"
"        highp vec2 XY;\n"

"void main(void) {\n"
"    XY = a_xy * u_pxsize;\n"
"    gl_Position = vec4(XY.x, XY.y, 1.0 , 1.0);\n"
"}";

constexpr std::string_view drawline_frag_src = "# version 300 es\n"
"uniform highp vec4 u_color;\n"
"out     highp vec4 fragColor;\n"

"void main(void) {\n"
"    fragColor = u_color;\n"
"}";

constexpr std::string_view kf_vert_src = "# version 300 es\n"
"in      highp vec4  a_fog;\n"
"in      highp vec3  a_xyz;\n"
"in      highp vec2  a_uv;\n"
"in      lowp  float a_z;\n"
"uniform highp vec2  u_pxsize;\n"
"        highp vec2  XY;\n"
"        highp float z;\n"
"out     highp vec4  v_fog;\n"
"out     highp vec2  v_uv;\n"
"out     highp float v_z;\n"

"void main(void) {\n"
"    z = 1.0 / a_xyz.z;\n"
"    XY  = a_xyz.xy * z * (2.0 / u_pxsize);\n"

"    v_fog = a_fog * z;\n"
"    v_uv  = a_uv  * z;\n"
"    v_z   = z;\n"
"    gl_Position = vec4(XY.x, -XY.y, a_z, 1.0);\n"
"}";

constexpr std::string_view kf_frag_src = "# version 300 es\n"
"in      highp vec4  v_fog;\n"
"in      highp vec2  v_uv;\n"
"in      highp float v_z;\n"
"uniform sampler2D   u_tex;\n"
"uniform highp vec2  u_imsize;\n"
"        highp vec4  FOG;\n"
"        highp vec2  UV;\n"
"        highp float z;\n"
"out     highp vec4  fragColor;\n"

"void main(void) {\n"
"    z   = 1.0 / v_z;\n"
"    FOG = v_fog * z;\n"
"    UV  = v_uv  * z * u_imsize;\n"
"    fragColor = texture(u_tex, UV) * FOG;\n"
"}";

constexpr std::string_view vram_vert = "# version 300 es\n"
"in      highp vec2 a_xy;\n"
"in      highp vec2 a_uv;\n"
"uniform highp vec2 u_pxsize[2];\n"
"        highp vec2 XY;\n"
"out     highp vec2 v_uv;\n"

"void main(void) {\n"
"    v_uv = a_uv;\n"
"    // convert 0.0 to 1.0 => -1.0 to +1.0\n"
"    XY = (a_xy * u_pxsize[0] * 2.0) - 1.0;\n"
"    gl_Position = vec4(XY.x, XY.y, 1.0, 1.0);\n"
"}";

constexpr std::string_view vram_frag = "# version 300 es\n"
"in      highp vec2 v_uv;\n"
"uniform sampler2D  u_tex;\n"
"uniform highp vec2 u_pxsize[2];\n"
"        highp vec2 UV;\n"
"out     highp vec4 fragColor;\n"

"void main(void) {\n"
"    UV = v_uv * u_pxsize[1];\n"
"    fragColor = texture(u_tex, UV);\n"
"}";
// clang-format on

static void checkCompileErrors(GLuint object);
static void checkLinkErrors(GLuint object);

const Shader& GetVRamShader() {
	static Shader shader;
	if (!shader) {
		shader.Compile(vram_vert, vram_frag);
	}
	return shader;
}
const Shader& GetKeyframeShader() {
	static Shader shader;
	if (!shader) {
		shader.Compile(kf_vert_src, kf_frag_src);
	}
	return shader;
}
const Shader& GetDrawlineShader() {
	static Shader shader;
	if (!shader) {
		shader.Compile(drawline_vert_src, drawline_frag_src);
	}
	return shader;
}

template <uint32_t ShaderType>
GLuint Compile_(const std::string_view source) {
	if (source.empty()) {
		return 0;
	}
	GLuint shader = glCreateShader(ShaderType);
	const char* ptr = source.data();
	int32_t size = source.size();
	glShaderSource(shader, 1, &ptr, &size);
	glCompileShader(shader);
	return shader;
}

Shader::Shader() noexcept
	: _id(0) {}
Shader::~Shader() noexcept { glDeleteProgram(_id); }

void Shader::Compile(std::string_view vertexSrc, std::string_view fragmentSrc) {
	GLuint sVertex = Compile_<GL_VERTEX_SHADER>(vertexSrc);
	checkCompileErrors(sVertex);
	GLuint sFragment = Compile_<GL_FRAGMENT_SHADER>(fragmentSrc);
	checkCompileErrors(sFragment);

	// shader program
	_id = glCreateProgram();
	glAttachShader(_id, sVertex);
	glAttachShader(_id, sFragment);
	glLinkProgram(_id);
	checkLinkErrors(_id);
	// delete the shaders as they're linked into our program now and no longer necessary
	glDeleteShader(sVertex);
	glDeleteShader(sFragment);
}

Shader& Shader::Use() {
	glUseProgram(_id);
	return *this;
}

const Shader& Shader::Use() const {
	glUseProgram(_id);
	return *this;
}

void Shader::SetUniform(const char* name, float value) const { glUniform1f(glGetUniformLocation(_id, name), value); }
void Shader::SetUniform(const char* name, int value) const { glUniform1i(glGetUniformLocation(_id, name), value); }
void Shader::SetUniform(const char* name, float x, float y) const {
	glUniform2f(glGetUniformLocation(_id, name), x, y);
}
void Shader::SetUniform(const char* name, const glm::vec2& value) const { SetUniform(name, value.x, value.y); }
void Shader::SetUniform(const char* name, float x, float y, float z) const {
	glUniform3f(glGetUniformLocation(_id, name), x, y, z);
}
void Shader::SetUniform(const char* name, const glm::vec3& value) const { SetUniform(name, value.x, value.y, value.z); }
void Shader::SetUniform(const char* name, float x, float y, float z, float w) const {
	glUniform4f(glGetUniformLocation(_id, name), x, y, z, w);
}
void Shader::SetUniform(const char* name, const glm::vec4& value) const {
	SetUniform(name, value.x, value.y, value.z, value.w);
}
void Shader::SetUniform(const char* name, const glm::mat4& matrix) const {
	glUniformMatrix4fv(glGetUniformLocation(_id, name), 1, false, glm::value_ptr(matrix));
}

void Shader::SetUniform(const char* name, std::span<const float> values) const {
	glUniform1fv(glGetUniformLocation(_id, name), values.size(), values.data());
}
void Shader::SetUniform(const char* name, std::span<const glm::vec2> values) const {
	glUniform2fv(glGetUniformLocation(_id, name), values.size(), &(values.data()[0][0]));
}
void Shader::SetUniform(const char* name, std::span<const glm::vec3> values) const {
	glUniform3fv(glGetUniformLocation(_id, name), values.size(), &(values.data()[0][0]));
}
void Shader::SetUniform(const char* name, std::span<const glm::vec4> values) const {
	glUniform4fv(glGetUniformLocation(_id, name), values.size(), &(values.data()[0][0]));
}
void Shader::SetUniform(const char* name, std::span<const int> values) const {
	glUniform1iv(glGetUniformLocation(_id, name), values.size(), values.data());
}
void Shader::SetUniform(const char* name, std::span<const glm::ivec2> values) const {
	glUniform2iv(glGetUniformLocation(_id, name), values.size(), &(values.data()[0][0]));
}
void Shader::SetUniform(const char* name, std::span<const glm::ivec3> values) const {
	glUniform3iv(glGetUniformLocation(_id, name), values.size(), &(values.data()[0][0]));
}
void Shader::SetUniform(const char* name, std::span<const glm::ivec4> values) const {
	glUniform4iv(glGetUniformLocation(_id, name), values.size(), &(values.data()[0][0]));
}

void Shader::SetVertexAttribute(unsigned int index, float* v, size_t v_sz, int n_component) const {
	GLuint buffer;
	glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, v_sz * sizeof(float), v, GL_STATIC_DRAW);
	glEnableVertexAttribArray(index);
	glVertexAttribPointer(index, n_component, GL_FLOAT, false, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

int Shader::GetAttribLocation(const char* name) const { return glGetAttribLocation(_id, name); }

int Shader::GetUniformLocation(const char* name) const { return glGetUniformLocation(_id, name); }

// checks if compilation or linking failed and if so, print the error logs
void checkCompileErrors(GLuint object) {
	char infoLog[1024];
	int success;
	glGetShaderiv(object, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(object, sizeof(infoLog), NULL, infoLog);
		std::cout << "| ERROR::Shader: Compile-time error:\n"
				  << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
	}
}

void checkLinkErrors(GLuint object) {
	char infoLog[1024];
	int success;
	glGetShaderiv(object, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(object, sizeof(infoLog), NULL, infoLog);
		std::cout << "| ERROR::Shader: Link-time error:\n"
				  << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
	}
}
