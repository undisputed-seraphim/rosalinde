#pragma once

#include "camera.hpp"
#include "glxx/buffers.hpp"
#include "sprite.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

class SpriteRenderer {
public:
	SpriteRenderer();
	~SpriteRenderer();
	SpriteRenderer(const SpriteRenderer&) = delete;

	void upload_textures(const SpriteData& data);
	void draw(const SpriteInstance& inst, const glm::mat4& projection, const Camera& cam, const glm::vec2& offset = {});

private:
	GLuint _texture_array = 0;
	GLuint _vao = 0;
	gl::ArrayBuffer<SpriteVertex> _vbo;
	gl::uiElementBuffer _ebo;

	std::vector<SpriteVertex> _verts;
	std::vector<uint32_t> _indices;
};
