#pragma once

#include "sprite_renderer.hpp"

#include <memory>
#include <string>
#include <vector>

class AssetLoader;

class MpdAsset {
public:
	static std::unique_ptr<MpdAsset> load(const AssetLoader& loader, const std::string& mpd, const std::string& ftx);

	void draw(const glm::mat4& projection, const Camera& camera);

private:
	SpriteRenderer _renderer;
	std::vector<SpriteVertex> _verts;
	std::vector<uint32_t> _indices;
};
