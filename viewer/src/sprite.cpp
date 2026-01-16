#include "sprite.hpp"
#include "shader.hpp"

#include <glm/ext.hpp>

static GLuint make_texture_array(std::vector<FTX::Entry> textures) {
	unsigned int max_x = 0, max_y = 0;
	for (auto& t : textures) {
		FTX::decompress(t);
		FTX::deswizzle(t);
		max_x = std::max(max_x, t.width);
		max_y = std::max(max_y, t.height);
		std::cout << t.name << '\t' << t.width << 'x' << t.height << '\n';
	}
	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D_ARRAY, id);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_ALWAYS);

	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, max_x, max_y, textures.size());
	for (int i = 0; i < textures.size(); ++i) {
		const auto& t = textures[i];
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, t.width, t.height, 1, GL_RGBA, GL_UNSIGNED_BYTE, t.rgba.data());
	}
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	return id;
}

static glm::mat4 s7_matrix(const mbs::section_7& s7, const bool flipx, const bool flipy) {
	const int8_t x = flipx ? -1 : 1;
	const int8_t y = flipy ? -1 : 1;
	glm::mat4 m{1.0};
	m = glm::scale(m, glm::vec3{s7.scale.x * x, s7.scale.y * y, 1.0});
	m = glm::translate(m, glm::vec3{s7.move.x * x, s7.move.y * y, s7.move.z});
	m *= glm::eulerAngleXYZ(s7.rotate.x, s7.rotate.y, s7.rotate.z);
	// NOTE: Normally the right order for this is scale-rotate-translate,
	// however accessories seem to be wrongly placed.
	// So scale-translate-rotate appears to get us closest to the right image.
	// I think there is some parent-child transform hierarchy that's currently missing.
	return m;
}

Sprite::Sprite(MBS mbs, std::vector<FTX::Entry> textures, uint32_t flags, uint32_t trackid)
	: _mbs(std::move(mbs))
	, _textures(std::move(textures))
	, _glTexHandle(0)
	, _flags(flags)
	, _trackidx(trackid) {
	_glTexHandle = make_texture_array(_textures);
	play(_trackidx);
}

void Sprite::play(uint32_t trackid) {
	_trackidx = trackid;

	const auto& s9 = _mbs.get().s9[_trackidx];
	_frames = std::vector<uint32_t>(s9.sa_set_no, 0);
	_track = std::vector<uint32_t>(s9.sa_set_no, 0);
}

void Sprite::render(Camera& cam, const glm::mat4& projection) {
	const auto& shader = GetKeyframeShader().Use();
	const auto& v77 = _mbs.get();
	const auto& s9 = v77.s9[_trackidx];

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, _glTexHandle);

	for (uint16_t i = 0; i < s9.sa_set_no; ++i) {
		const auto& sa = v77.sa[s9.sa_set_id + i];
		const auto& s8 = v77.s8[sa.s8_id + _track[i]];
		const auto& s7 = v77.s7[s8.s7_id]; // Matrix
		const auto& s6 = v77.s6[s8.s6_id]; // Keyframe
		// const auto& s5 = v77.s5[s6.s5_id]; // Hitbox
		// const auto& s3 = v77.s3[s5.s3_id]; // Hitbox Matrix

		if (s8.flags & mbs::v77::s8flag::HITBOX) { // Don't care about hitbox for now
			continue;
		}

		if (s6.s4_no == 0) { // Ignore if frame does not have any content in it
			continue;
		}

		if (_frames[i] == 0) {
			_frames[i] = v77.s8[sa.s8_id + _track[i]].frames;
			if (++_track[i] == sa.s8_no) {
				_track[i] = 0;
			}
		}
		_frames[i]--;

		_vertices.storage().clear();
		_indices.storage().clear();

		// Draw each layer
		const float zrate = 1.0f / (s6.s4_no + 1);
		float depth = 1.0;
		uint32_t l = 0;
		for (uint32_t j = 0; j < s6.s4_no; ++j) { // For each layer
			const auto& s4 = v77.s4[s6.s4_id + j];
			if ((s4.attributes & ~_flags) != 0) {
				continue;
			}

			const auto& tex = _textures[s4.tex_id];
			const auto texdim = glm::vec2{tex.width, tex.height};
			for (int k = 0; k < 4; ++k) {
				const auto& dst = v77.s2[s4.s2_id].values[k];
				const auto& src = v77.s1[s4.s1_id].values[k];
				const auto& fog = v77.s0[s4.s0_id].colors[k];
				_vertices.storage().emplace_back(vertex{s4.tex_id, src * texdim, glm::vec3{dst, depth}, fog});
			}
			depth -= zrate;
			_indices.storage().insert(_indices.storage().end(), {l + 0, l + 1, l + 3, l + 1, l + 2, l + 3});
			l += 4;
		}

		const bool flipx = mbs::v77::s8flag::FLIPX & s8.flags;
		const bool flipy = mbs::v77::s8flag::FLIPY & s8.flags;
		const auto s7m = s7_matrix(s7, flipx, flipy);

		shader.SetUniform("u_mvp", projection * cam.lookAt() * s7m);
		shader.SetUniform("u_tex", 0);
		_vertices.bind().setData(gl::buffer::Usage::STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 1, GL_SHORT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, texid));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, uv));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, xyz));
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void*)offsetof(vertex, color));

		_indices.bind().setData(gl::buffer::Usage::STATIC_DRAW).drawElements(gl::Mode::TRIANGLES);
	}
}

void Sprite::update(uint64_t delta) {

}
