#include <glad/glad.h>
#include <glm/ext.hpp>
#include <iostream>

#include "glxx/error.hpp"
#include "glxx/shader.hpp"

static void checkCompileErrors(GLuint object);
static void checkLinkErrors(GLuint object);

template <uint32_t ShaderType>
static GLuint Compile_(const std::string_view source) {
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
	char infoLog[512];
	GLint success;
	glGetShaderiv(object, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE) {
		glGetShaderInfoLog(object, sizeof(infoLog), NULL, infoLog);
		std::cout << "| ERROR::Shader: Compile-time error:\n" << infoLog << std::endl;
	}
}

void checkLinkErrors(GLuint object) {
	char infoLog[512];
	GLint success;
	glGetProgramiv(object, GL_LINK_STATUS, &success);
	if (success != GL_TRUE) {
		glGetProgramInfoLog(object, sizeof(infoLog), NULL, infoLog);
		std::cout << "| ERROR::Shader: Link-time error:\n" << infoLog << std::endl;
	}
}
