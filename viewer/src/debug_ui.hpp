#pragma once

#include "asset_loader.hpp"
#include "sprite_layer.hpp"

#include <memory>
#include <string>
#include <vector>

class DebugUI {
public:
	DebugUI();
	~DebugUI();

	void begin_frame();
	void draw(
		std::vector<std::unique_ptr<SpriteLayer>>& layers,
		const std::vector<std::string>& class_names,
		const AssetLoader& loader,
		int& variant_side);
	void end_frame();
};
