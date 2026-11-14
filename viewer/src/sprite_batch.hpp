#pragma once

#include "camera.hpp"
#include "sprite.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class SpriteBatch {
public:
	SpriteBatch();
	~SpriteBatch();
	SpriteBatch(const SpriteBatch&) = delete;
	SpriteBatch& operator=(const SpriteBatch&) = delete;
	SpriteBatch(SpriteBatch&&) = delete;
	SpriteBatch& operator=(SpriteBatch&&) = delete;

	bool matches(GLuint texture_array, const glm::mat4& projection, const Camera& camera) const;
	void begin(GLuint texture_array, const glm::mat4& projection, const Camera& camera);
	void append_instance(
		const SpriteInstance& instance,
		const glm::mat4& model,
		const glm::vec4* tint,
		bool apply_fog,
		float alpha,
		uint32_t layer_mask);
	void flush();
	uint64_t draws() const { return _draws; }
	void reset_draws() { _draws = 0; }

private:
	void begin_blend(uint8_t blend);

	GLuint _vao = 0;
	GLuint _vbo = 0;
	GLuint _ebo = 0;
	GLuint _texture_array = 0;
	const glm::mat4* _projection = nullptr;
	const Camera* _camera = nullptr;
	glm::mat4 _mvp{1.0f};
	uint8_t _blend = 0xFF;
	uint64_t _draws = 0;
	std::vector<SpriteVertex> _verts;
	std::vector<uint32_t> _indices;
};
