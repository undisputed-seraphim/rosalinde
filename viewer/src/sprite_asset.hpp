#pragma once

#include "sprite.hpp"
#include "sprite_renderer.hpp"

#include <cstdint>
#include <memory>
#include <string>

// Behaviour class of a sprite. The asset path supplies the default; the native
// that created an entity can refine it (a Chara/ asset can be a world character
// or a UI character); see sprite_kind.hpp.
enum class SpriteKind : uint8_t {
	Generic,
	Character,
	UiCharacter,
	Background,
	Mone,
};

constexpr bool is_character_kind(SpriteKind kind) {
	return kind == SpriteKind::Character || kind == SpriteKind::UiCharacter;
}

struct SpriteAsset {
	std::string mbs;
	std::string ftx;
	SpriteData data;
	SpriteRenderer renderer;
	SpriteKind kind = SpriteKind::Generic;
};

using SpriteAssetPtr = std::shared_ptr<SpriteAsset>;
