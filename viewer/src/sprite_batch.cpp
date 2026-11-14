#include "sprite_batch.hpp"
#include "shader.hpp"
#include "sprite_renderer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <numeric>

SpriteBatch::SpriteBatch() {
	glGenVertexArrays(1, &_vao);
	glGenBuffers(1, &_vbo);
	glGenBuffers(1, &_ebo);

	glBindVertexArray(_vao);
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);

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

SpriteBatch::~SpriteBatch() {
	if (_ebo)
		glDeleteBuffers(1, &_ebo);
	if (_vbo)
		glDeleteBuffers(1, &_vbo);
	if (_vao)
		glDeleteVertexArrays(1, &_vao);
}

bool SpriteBatch::matches(GLuint texture_array, const glm::mat4& projection, const Camera& camera) const {
	return _texture_array == texture_array && _projection == &projection && _camera == &camera;
}

void SpriteBatch::begin(GLuint texture_array, const glm::mat4& projection, const Camera& camera) {
	_texture_array = texture_array;
	_projection = &projection;
	_camera = &camera;
	_mvp = projection * camera.lookAt();
	_blend = 0xFF;
	_verts.clear();
	_indices.clear();
}

void SpriteBatch::begin_blend(uint8_t blend) {
	if (_blend == blend)
		return;
	flush();
	_blend = blend;
}

void SpriteBatch::append_instance(
	const SpriteInstance& instance,
	const glm::mat4& model,
	const glm::vec4* tint,
	bool apply_fog,
	float alpha,
	uint32_t layer_mask) {
	if (!instance.data)
		return;
	const uint32_t count = instance.sa_count();
	if (count == 0)
		return;

	auto append_run = [&](uint32_t run) {
		SpriteRunGeometry geometry;
		if (!instance.prepare_run_geometry(run, model, layer_mask, geometry))
			return;
		for (uint8_t blend = 0; blend < 4; ++blend) {
			if (!geometry.visible[blend])
				continue;
			begin_blend(blend);
			instance.append_run(blend, geometry, tint, apply_fog, alpha, layer_mask, _verts, _indices);
		}
	};

	static const char* const run_order_env = std::getenv("VIEWER_RUN_ORDER");
	if (!run_order_env) {
		for (uint32_t run = 0; run < count; ++run)
			append_run(run);
		return;
	}

	std::vector<uint32_t> order(count);
	std::iota(order.begin(), order.end(), 0u);
	if (std::strcmp(run_order_env, "reverse") == 0) {
		std::reverse(order.begin(), order.end());
	} else if (std::strcmp(run_order_env, "track") == 0 && instance.playback.track_idx < instance.data->tracks.size()) {
		const auto& runs = instance.data->tracks[instance.playback.track_idx].runs;
		std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
			return a < runs.size() && b < runs.size() && runs[a].sa_track_slot < runs[b].sa_track_slot;
		});
	}
	for (uint32_t run : order)
		append_run(run);
}

void SpriteBatch::flush() {
	if (_verts.empty()) {
		_blend = 0xFF;
		return;
	}

	GetKeyframeShader().Use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _texture_array);
	glBindVertexArray(_vao);

	const auto& uniforms = GetKeyframeUniforms();
	glUniform1i(uniforms.tex, 0);
	glUniform1f(uniforms.alpha, 1.0f);
	glUniformMatrix4fv(uniforms.mvp, 1, GL_FALSE, glm::value_ptr(_mvp));

	set_sprite_blend_mode(_blend);
	glBindBuffer(GL_ARRAY_BUFFER, _vbo);
	glBufferData(
		GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(_verts.size() * sizeof(SpriteVertex)), _verts.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
	glBufferData(
		GL_ELEMENT_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(_indices.size() * sizeof(uint32_t)),
		_indices.data(),
		GL_STREAM_DRAW);
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_indices.size()), GL_UNSIGNED_INT, nullptr);
	++_draws;

	_verts.clear();
	_indices.clear();
	_blend = 0xFF;
}
