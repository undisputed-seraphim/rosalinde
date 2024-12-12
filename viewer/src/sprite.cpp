#include <glad/glad.h>
#include <glm/glm.hpp>

#include "shader.hpp"
#include "sprite.hpp"

static_assert(std::is_same_v<GLuint, uint32_t>);

// clang-format off
auto camera2 = glm::mat4{
	2, 0, 0, 0,
	0, 2, 0, 322,
	0, 0, 1, 0,
	0, 0, 0, 1
};
// clang-format on

// IN:  Quad vertices
// OUT: Triangle vertices, transformed
static glm::mat4x3 transform(glm::mat4x2 vertices) {
	// clang-format off
	camera2[0][0] = 2.07219708396179;
	camera2[1][1] = 2.07219708396179;

	const auto v2 = glm::mat4{
		glm::vec4(vertices[0], 1.0, 1.0),
		glm::vec4(vertices[1], 1.0, 1.0),
		glm::vec4(vertices[2], 1.0, 1.0),
		glm::vec4(vertices[3], 1.0, 1.0),
	};

	const glm::mat4x3 v = glm::transpose(camera2) * v2;
	const auto h = glm::transpose(glm::mat3{
		glm::cross(glm::cross(v[0], v[2]), glm::cross(v[1], v[3])),
		glm::cross(glm::cross(v[0], v[1]), glm::cross(v[3], v[2])),
		glm::cross(glm::cross(v[0], v[3]), glm::cross(v[1], v[2]))
	});

	//perspective_quad
	const auto h_inv = glm::mat3{
		 0,     0,  0.005,
	-0.001,     0,  0.015,
		 0, 0.001, -0.015
	};
	const glm::mat3 m3 = h_inv * h;
	const auto t = glm::mat4x3{
		glm::vec3{10, 10, 1} * m3,
		glm::vec3{20, 10, 1} * m3,
		glm::vec3{20, 20, 1} * m3,
		glm::vec3{10, 20, 1} * m3,
	};
	// clang-format on
	return t;
}

static glm::mat4x2 transformUV(glm::mat4x2 uv, const std::vector<FTX::Entry>& textures, int16_t texid) {
	const auto& t = textures[texid];
	const glm::vec2 dims{t.width, t.height};
	glm::vec2 d(0.0);
	for (int16_t i = 0; i < textures.size(); ++i) {
		if (i == texid) {
			for (int j = 0; j < 4; ++j) {
				uv[j] *= dims;
				uv[j][0] += d[0];

				// Due to some weird error, the x-axis of the texture
				// must be shifted right by 2, to avoid artifacts.
				uv[j][0] += 2;
			}
		}
		d[0] += float(textures[i].width);
		d[1] = std::max(d[1], float(textures[i].height));
	}
	uv /= d;
	return uv;
}

Sprite::Sprite(Quad q)
	: _q(std::move(q)) {}

void Sprite::Animate(int current_active_anim) {
	static int timestep = 0;

	const auto& a = _q.animations().at(current_active_anim);
	const auto& tl = a.timelines.at(timestep++);
	timestep %= a.timelines.size();

	switch (tl.attach.objt) {
	case Quad::ObjectType::KEYFRAME: {
		AnimateKeyframe(_q.keyframes()[tl.attach.id]);
		break;
	}
	}
}

void Sprite::AnimateKeyframe(const Quad::Keyframe& kf) {
	GLint W, H;
	glGetIntegeri_v(GL_VIEWPORT, 2, &W);
	glGetIntegeri_v(GL_VIEWPORT, 3, &H);

	xyz.clear();
	uv.clear();
	fog.clear();
	z.clear();
	indices.clear();

	const float zrate = 1.0 / (kf.layers.size() + 1);
	float depth = 1.0;
	unsigned i = 0;
	for (const auto& layer : kf.layers) {
		if (layer.attribute & mask) {
			continue;
		}
		xyz.push_back(transform(layer.dst));
		indices.insert(indices.end(), {i + 0, i + 1, i + 2, i + 0, i + 2, i + 3});
		fog.insert(fog.end(), layer.fog.begin(), layer.fog.end());

		depth -= zrate;
		z.insert(z.end(), {depth, depth, depth, depth});
		i += 4;
	}

	const auto& shader = GetKeyframeShader().Use();
	shader.SetUniform("u_pxsize", {(float)W, (float)H});
	shader.SetUniform("u_tex", 0);

	glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * fog.size(), &fog[0][0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4x3) * xyz.size(), &xyz[0][0][0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, VBO[2]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4x2) * uv.size(), &uv[0][0][0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, VBO[3]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * z.size(), z.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned) * indices.size(), indices.data(), GL_STATIC_DRAW);

	glDrawElements(GL_TRIANGLES, xyz.size() * 4 * 3, GL_UNSIGNED_INT, 0);
}