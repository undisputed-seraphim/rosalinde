#pragma once

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <vector>

#include "shader.hpp"
#include "texture.hpp"
#include <eltolinde.hpp>

class Sprite {
public:
	Sprite(Quad q);
	Sprite(const Sprite&) = delete;
	~Sprite() noexcept = default;

	void AddTexture(Texture2D&&);

	void Animate(int anim) const;

private:
	Quad _q;
	std::vector<Texture2D> _textures;
};