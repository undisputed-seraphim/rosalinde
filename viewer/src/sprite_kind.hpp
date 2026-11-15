#pragma once

#include "sprite_asset.hpp"

#include <scarlett/scene.hpp>

// One classification shared by playback, rendering and diagnostics. The entity
// kind wins because the same Chara/ asset can be a world character, a UI
// character, or an extra sprite depending on the native that created it; other
// entities fall back to the asset path class.
inline SpriteKind sprite_entity_kind(const asb::RuntimeEntity& e) {
	if (e.kind == "DrCreateChara" || e.kind == "DrCreateNpc")
		return SpriteKind::Character;
	if (e.kind == "DrUiCreateChara")
		return SpriteKind::UiCharacter;
	if (e.kind.rfind("CreateSpriteMone", 0) == 0)
		return SpriteKind::Mone;
	return SpriteKind::Generic;
}

inline SpriteKind sprite_kind_for(const asb::RuntimeEntity& e, const SpriteAsset& asset) {
	const SpriteKind kind = sprite_entity_kind(e);
	return kind == SpriteKind::Generic ? asset.kind : kind;
}
