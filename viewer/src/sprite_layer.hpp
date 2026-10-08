#pragma once

#include "sprite.hpp"
#include "sprite_renderer.hpp"

#include <criware/cpk.hpp>
#include <map>
#include <vector>

struct SpriteLayer {
	std::string name;
	std::string class_name;
	std::string variant_name;
	glm::vec2 position{0, 0};
	SpriteData data;
	SpriteInstance instance;
	SpriteRenderer renderer;
	uint32_t default_flags = 0;
	std::map<uint32_t, glm::vec4> layer_tints;
};

struct BackgroundScene {
	struct Element {
		uint32_t track_idx;
		bool is_far;
		SpriteInstance instance;
	};
	SpriteData data;
	SpriteRenderer renderer;
	std::vector<Element> elements;

	void load(const struct Job& job, CPKTable& cpkt);
	void update(float dt);
	glm::vec4 extent() const;
};
