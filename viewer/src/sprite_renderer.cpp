#include "sprite_renderer.hpp"
#include "shader.hpp"

#include <glm/ext.hpp>

SpriteRenderer::SpriteRenderer() {
	glGenVertexArrays(1, &_vao);
	glBindVertexArray(_vao);

	_vbo.bind();
	_ebo.bind();

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 1, GL_SHORT, GL_FALSE, sizeof(SpriteVertex), (void*)offsetof(SpriteVertex, texid));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void*)offsetof(SpriteVertex, uv));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void*)offsetof(SpriteVertex, xyz));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(SpriteVertex), (void*)offsetof(SpriteVertex, color));

	glBindVertexArray(0);
}

SpriteRenderer::~SpriteRenderer() {
	if (_vao) glDeleteVertexArrays(1, &_vao);
	if (_texture_array) glDeleteTextures(1, &_texture_array);
}

void SpriteRenderer::upload_textures(const SpriteData& data) {
	uint32_t max_x = 0, max_y = 0;
	for (const auto& t : data.textures) {
		max_x = std::max(max_x, t.width);
		max_y = std::max(max_y, t.height);
	}

	if (_texture_array) glDeleteTextures(1, &_texture_array);
	glGenTextures(1, &_texture_array);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _texture_array);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_ALWAYS);

	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, max_x, max_y, data.textures.size());
	for (size_t i = 0; i < data.textures.size(); ++i) {
		const auto& t = data.textures[i];
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, t.width, t.height, 1, GL_RGBA, GL_UNSIGNED_BYTE, t.rgba.data());
	}
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void SpriteRenderer::draw(const SpriteInstance& inst, const glm::mat4& projection, const Camera& cam,
	const glm::vec2& offset, const std::map<uint32_t, glm::vec4>* tints) {
	const auto& shader = GetKeyframeShader().Use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _texture_array);
	glBindVertexArray(_vao);
	shader.SetUniform("u_tex", 0);

	const glm::mat4 view_offset = cam.lookAt() * glm::translate(glm::mat4(1.0f), glm::vec3(offset, 0.0f));
	uint32_t n = inst.sa_count();
	for (uint32_t i = 0; i < n; ++i) {
		auto s7m = inst.transform_for_sa(i);
		shader.SetUniform("u_mvp", projection * view_offset * s7m);

		inst.build_vertices(i, _verts, _indices, tints);
		if (_verts.empty()) continue;

		_vbo.bind().setData(gl::buffer::Usage::STATIC_DRAW, std::span(_verts));
		_ebo.bind().setData(gl::buffer::Usage::STATIC_DRAW, std::span(_indices));
		_ebo.drawElements(gl::Mode::TRIANGLES);
	}
}
