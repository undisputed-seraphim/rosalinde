#pragma once

#include "sprite.hpp"
#include "sprite_profile.hpp"
#include "sprite_renderer.hpp"

#include <memory>
#include <string>

struct SpriteAsset {
	std::string mbs;
	std::string ftx;
	SpriteData data;
	SpriteRenderer renderer;
	SpriteKind kind = SpriteKind::Generic;
};

using SpriteAssetPtr = std::shared_ptr<SpriteAsset>;
