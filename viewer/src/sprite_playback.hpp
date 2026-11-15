#pragma once

#include "sprite_asset.hpp"

#include <scarlett/scene.hpp>

#include <vector>

// Owns the instances for one sprite asset. Characters and backgrounds share
// this path; the SpriteProfile decides track selection and advance rules.
class SpritePlayback {
public:
	SpriteAssetPtr asset;
	std::vector<SpriteInstance> instances;
	int track_index = -1;
	long applied_start_frame = -1;
	float rate = 1.0f;

	void reset(SpriteAssetPtr asset, const SpriteProfile& profile, const asb::RuntimeEntity& state, bool flip_v);
	void apply(const asb::RuntimeEntity& state);
	void apply_track_order(const std::vector<uint32_t>& order);
	void advance(float dt);

	bool empty() const { return instances.empty(); }

private:
	SpriteProfile _profile;

	static int choose_track(const SpriteData& data, const asb::RuntimeEntity& state);
	static bool track_usable(const SpriteData& data, int index);
};
