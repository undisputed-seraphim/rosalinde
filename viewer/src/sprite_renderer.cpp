#include "sprite_renderer.hpp"
#include "screenshot.hpp"
#include "shader.hpp"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <glm/ext.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <span>
#include <string>
#include <utility>

void set_sprite_blend_mode(uint8_t b) {
	glBlendEquation(GL_FUNC_ADD);
	switch (b) {
	case 1: // additive
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		break;
	case 2: // multiply
		glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case 3: // screen
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		break;
	default: // normal alpha
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	}
}

SpriteRenderer::SpriteRenderer() {
	glGenVertexArrays(1, &_vao);
	glBindVertexArray(_vao);

	_vbo.bind();
	_ebo.bind();

	glEnableVertexAttribArray(0);
	glVertexAttribIPointer(0, 1, GL_SHORT, sizeof(SpriteVertex), (void*)offsetof(SpriteVertex, texid));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void*)offsetof(SpriteVertex, uv));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void*)offsetof(SpriteVertex, xyz));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SpriteVertex), (void*)offsetof(SpriteVertex, color));

	glBindVertexArray(0);
}

SpriteRenderer::SpriteRenderer(SpriteRenderer&& other) noexcept
	: _texture_array(std::exchange(other._texture_array, 0))
	, _vao(std::exchange(other._vao, 0))
	, _deswizzler(std::move(other._deswizzler))
	, _vbo(std::move(other._vbo))
	, _ebo(std::move(other._ebo)) {}

SpriteRenderer& SpriteRenderer::operator=(SpriteRenderer&& other) noexcept {
	if (this != &other) {
		if (_vao)
			glDeleteVertexArrays(1, &_vao);
		if (_texture_array)
			glDeleteTextures(1, &_texture_array);
		_texture_array = std::exchange(other._texture_array, 0);
		_vao = std::exchange(other._vao, 0);
		_deswizzler = std::move(other._deswizzler);
		_vbo = std::move(other._vbo);
		_ebo = std::move(other._ebo);
	}
	return *this;
}

SpriteRenderer::~SpriteRenderer() {
	if (_vao)
		glDeleteVertexArrays(1, &_vao);
	if (_texture_array)
		glDeleteTextures(1, &_texture_array);
}

void SpriteRenderer::upload_textures(const SpriteData& data, std::string_view label) {
	upload_textures(data.textures, label);
}

void SpriteRenderer::upload_textures(std::span<const FTX::Entry> textures, std::string_view label) {
	uint32_t max_x = 0, max_y = 0;
	for (const auto& t : textures) {
		max_x = std::max(max_x, t.width);
		max_y = std::max(max_y, t.height);
	}

	if (_texture_array)
		glDeleteTextures(1, &_texture_array);
	glGenTextures(1, &_texture_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _texture_array);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_ALWAYS);

	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, max_x, max_y, textures.size());

	if (!_deswizzler)
		_deswizzler = std::make_unique<Deswizzler>();
	for (size_t i = 0; i < textures.size(); ++i) {
		const auto& t = textures[i];
		const size_t need = static_cast<size_t>(t.compressed_size) + t.uncompressed_size;
		if (t.width == 0 || t.height == 0 || t.rgba.size() < need) {
			spdlog::warn(
				"tex: skip malformed texture {} ({}x{}, {} bytes, need {})", i, t.width, t.height, t.rgba.size(), need);
			continue;
		}
		const auto* raw = reinterpret_cast<const uint8_t*>(t.rgba.data());
		std::span<const uint8_t> payload(raw + t.compressed_size, t.uncompressed_size);
		_deswizzler->upload_slice(_texture_array, static_cast<unsigned>(i), t.width, t.height, t.comp, payload);
	}
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	const char* dump_dir = std::getenv("FTX_GPU_DUMP_DIR");
	const char* match = std::getenv("FTX_GPU_DUMP_MATCH");
	if (dump_dir && (!match || label.find(match) != std::string_view::npos)) {
		const size_t pixels = static_cast<size_t>(max_x) * max_y * textures.size();
		if (pixels == 0 || pixels > 64u * 1024u * 1024u) {
			spdlog::warn("texture dump skipped: {} ({} pixels)", label, pixels);
		} else {
			std::filesystem::create_directories(dump_dir);
			std::string name(label);
			for (char& c : name)
				if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-')
					c = '_';
			if (name.empty())
				name = "texture";
			std::vector<uint8_t> pixels_rgba(pixels * 4);
			glBindTexture(GL_TEXTURE_2D_ARRAY, _texture_array);
			glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels_rgba.data());
			const auto path = std::filesystem::path(dump_dir) / (name + ".png");
			if (screenshot::write(path.string(), max_x, max_y * textures.size(), pixels_rgba))
				spdlog::info("texture dump: {}", path.string());
			else
				spdlog::error("texture dump failed: {}", path.string());
			glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
		}
	}
}

void SpriteRenderer::draw_raw(
	std::span<const SpriteVertex> verts,
	std::span<const uint32_t> indices,
	const glm::mat4& mvp,
	float alpha) {
	if (verts.empty() || indices.empty() || _texture_array == 0)
		return;
	GetKeyframeShader().Use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _texture_array);
	glBindVertexArray(_vao);
	const auto& uniforms = GetKeyframeUniforms();
	glUniform1i(uniforms.tex, 0);
	glUniform1f(uniforms.alpha, alpha);
	glUniformMatrix4fv(uniforms.mvp, 1, GL_FALSE, glm::value_ptr(mvp));
	set_sprite_blend_mode(0);
	const gl::ArrayBuffer<SpriteVertex>& vbo = _vbo;
	const gl::uiElementBuffer& ebo = _ebo;
	vbo.bind().setData(gl::buffer::Usage::STREAM_DRAW, verts);
	ebo.bind().setData(gl::buffer::Usage::STREAM_DRAW, indices);
	ebo.drawElements(gl::Mode::TRIANGLES, indices.size());
}
