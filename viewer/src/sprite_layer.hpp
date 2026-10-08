#pragma once

#include "sprite.hpp"
#include "sprite_renderer.hpp"

#include <algorithm>
#include <cmath>
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

	void update(float dt) {
		for (auto& el : elements)
			el.instance.update(dt);
	}

	glm::vec4 extent() const {
		float l = INFINITY, t = INFINITY, r = -INFINITY, b = -INFINITY;
		for (const auto& s9 : data.v77.s9) {
			if (s9.disabled)
				continue;
			l = std::min(l, s9.left);
			t = std::min(t, s9.top);
			r = std::max(r, s9.right);
			b = std::max(b, s9.bottom);
		}
		return {l, t, r, b};
	}

	glm::vec4 content_extent() const {
		float l = INFINITY, t = INFINITY, r = -INFINITY, b = -INFINITY;
		for (const auto& el : elements) {
			const glm::vec4 e = el.instance.content_bounds();
			l = std::min(l, e.x);
			t = std::min(t, e.y);
			r = std::max(r, e.z);
			b = std::max(b, e.w);
		}
		return {l, t, r, b};
	}

	void rebind() {
		for (auto& el : elements)
			el.instance.data = &data;
	}
};
