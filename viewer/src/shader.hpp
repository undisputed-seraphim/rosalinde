#pragma once

#include <glm/glm.hpp>
#include <string_view>
#include <span>

class Shader {
public:
	// constructor
	Shader() noexcept;
	Shader(const Shader&) = delete;
	Shader(Shader&&) noexcept = default;
	~Shader() noexcept;

	// compiles the shader from given source code
	void Compile(std::string_view vertexSource, std::string_view fragmentSource);

	// sets the current shader as active
	Shader& Use();
	const Shader& Use() const;

	// Sets uniform values for the shader program.
	void SetUniform(const char* name, float value) const;
	void SetUniform(const char* name, int value) const;
	void SetUniform(const char* name, float x, float y) const;
	void SetUniform(const char* name, const glm::vec2& value) const;
	void SetUniform(const char* name, float x, float y, float z) const;
	void SetUniform(const char* name, const glm::vec3& value) const;
	void SetUniform(const char* name, float x, float y, float z, float w) const;
	void SetUniform(const char* name, const glm::vec4& value) const;
	void SetUniform(const char* name, const glm::mat4& matrix) const;
	
	void SetUniform(const char* name, std::span<const float> values) const;
	void SetUniform(const char* name, std::span<const glm::vec2> values) const;
	void SetUniform(const char* name, std::span<const glm::vec3> values) const;
	void SetUniform(const char* name, std::span<const glm::vec4> values) const;
	void SetUniform(const char* name, std::span<const int> values) const;
	void SetUniform(const char* name, std::span<const glm::ivec2> values) const;
	void SetUniform(const char* name, std::span<const glm::ivec3> values) const;
	void SetUniform(const char* name, std::span<const glm::ivec4> values) const;

	void SetVertexAttribute(unsigned int index, float* v, size_t v_sz, int n_component) const;

	int GetAttribLocation(const char* name) const;
	int GetUniformLocation(const char* name) const;

	inline operator bool() const noexcept { return _id != 0; }

private:
	unsigned int _id;
};

const Shader& GetKeyframeShader();
