#pragma once

#include "camera.hpp"
#include "deswizzle.hpp"
#include "glxx/buffers.hpp"
#include "sprite.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

class SpriteRenderer {
public:
	SpriteRenderer();
	~SpriteRenderer();
	SpriteRenderer(const SpriteRenderer&) = delete;
	SpriteRenderer(SpriteRenderer&& other) noexcept;
	SpriteRenderer& operator=(SpriteRenderer&& other) noexcept;

	void upload_textures(const SpriteData& data, std::string_view label = {});
	void upload_textures(std::span<const FTX::Entry> textures, std::string_view label = {});
	GLuint texture_array() const { return _texture_array; }
	void draw_raw(
		std::span<const SpriteVertex> verts,
		std::span<const uint32_t> indices,
		const glm::mat4& mvp,
		float alpha = 1.0f);
	void draw(
		const SpriteInstance& inst,
		const glm::mat4& projection,
		const Camera& cam,
		const glm::vec2& offset = {},
		const std::map<uint32_t, glm::vec4>* tints = nullptr,
		bool apply_fog = false,
		float scale = 1.0f,
		float alpha = 1.0f,
		bool flip_x = false,
		float rotation = 0.0f,
		uint32_t layer_mask = 0xFFFFFFFFu,
		float scale_y = 0.0f);

private:
	GLuint _texture_array = 0;
	GLuint _vao = 0;
	std::unique_ptr<Deswizzler> _deswizzler;
	gl::ArrayBuffer<SpriteVertex> _vbo;
	gl::uiElementBuffer _ebo;

	std::vector<SpriteVertex> _verts;
	std::vector<uint32_t> _indices;
};
