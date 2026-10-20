#pragma once

#include "sprite_asset.hpp"

#include <scarlett/scene.hpp>

#include <vector>

enum class TrackPolicy {
	ScriptTrack,
	AllEnabledTracks,
};

// Owns the instances for one sprite asset. Characters and backgrounds share
// this path; only the track policy differs.
class SpritePlayback {
public:
	SpriteAssetPtr asset;
	std::vector<SpriteInstance> instances;
	int track_index = -1;
	float rate = 1.0f;

	void reset(
		SpriteAssetPtr asset,
		TrackPolicy policy,
		const asb::RuntimeEntity& state,
		bool flip_v,
		bool hold_finished,
		bool character = false);
	void apply(const asb::RuntimeEntity& state);
	void advance(float dt);

	bool empty() const { return instances.empty(); }

private:
	TrackPolicy _policy = TrackPolicy::ScriptTrack;

	static int choose_track(const SpriteData& data, const asb::RuntimeEntity& state);
	static bool track_usable(const SpriteData& data, int index);
};
