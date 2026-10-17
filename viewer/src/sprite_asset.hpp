#pragma once

#include "sprite.hpp"
#include "sprite_renderer.hpp"

#include <memory>
#include <string>

struct SpriteAsset {
	std::string mbs;
	std::string ftx;
	SpriteData data;
	SpriteRenderer renderer;
	bool is_character = false;
	bool is_background = false;
};

using SpriteAssetPtr = std::shared_ptr<SpriteAsset>;
