#pragma once

#include "asset_loader.hpp"
#include "sprite_layer.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

// A request to load one scene entity as a sprite layer, decoupled from whether
// it came from the static scene fold or the hosted-VM timeline.
struct LayerSpec {
	std::string id;	  // logical name (used for logging + the layer name)
	std::string kind; // originating create call (used as the MBS variant name)
	std::string mbs;
	std::string ftx;
	std::string track; // animation name (resolved when track_index < 0)
	int track_index = -1;
	glm::vec2 pos{0.0f, 0.0f};
	float scale = 1.0f;
	bool visible = true;
};

// Resolve each spec to a SpriteLayer. The result is aligned with `specs`
// (entry i corresponds to specs[i], nullptr when it was skipped or beyond
// `limit`). `limit` counts loaded layers; < 0 means unlimited.
std::vector<std::unique_ptr<SpriteLayer>>
build_layers(const AssetLoader& loader, const std::vector<LayerSpec>& specs, int limit);
