#pragma once

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <vector>

#include "texture.hpp"
#include <eltolinde.hpp>

class Sprite {
public:
	Sprite(Quad q);
	Sprite(const Sprite&) = delete;
	~Sprite() noexcept = default;

	void Animate(int anim);
	void AnimateKeyframe(const Quad::Keyframe& kf);

private:
	Quad _q;

	unsigned int VBO[5];
	unsigned int EBO;
	unsigned int tex;
	
	uint32_t mask;
	std::vector<glm::mat4x3> xyz;
	std::vector<glm::mat4x2> uv;
	std::vector<unsigned> indices;
	std::vector<uint32_t> fog;
	std::vector<float> z;
};