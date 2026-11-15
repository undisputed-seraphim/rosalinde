#pragma once

#include <cstdint>

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

// The handful of behaviours that used to be scattered booleans. Built once per
// playback object so the playback, advance and layer paths read named rules
// instead of repeating kind tests.
struct SpriteProfile {
	SpriteKind kind = SpriteKind::Generic;
	bool all_tracks = false;			  // play every enabled track (background)
	bool character = false;				  // world/UI chara
	bool hold_finished = false;			  // hold the last pose/frame instead of hiding
	bool draw_inactive_frames = false;	  // chara: draw non-ACTIVE transitional frames
	bool gate_main_sequence = false;	  // stop runs once the main sequence is done
	bool one_shot_ends = false;			  // a transition-mode one-shot terminates the run
	bool draws_standalone_blend1 = false; // background: blend=1 layer without its source draws
};

constexpr SpriteProfile sprite_profile(SpriteKind kind, bool stage_layer, bool stateful_mone) {
	SpriteProfile profile;
	profile.kind = kind;
	const bool background = kind == SpriteKind::Background;
	profile.character = is_character_kind(kind);
	profile.all_tracks = background && !stage_layer;
	profile.hold_finished = background || profile.character || stage_layer || stateful_mone;
	profile.draw_inactive_frames = profile.character;
	profile.gate_main_sequence = !profile.character;
	profile.one_shot_ends = !profile.character;
	profile.draws_standalone_blend1 = profile.all_tracks;
	return profile;
}
