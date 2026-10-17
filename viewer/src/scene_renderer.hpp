#pragma once

#include "asset_library.hpp"
#include "camera.hpp"
#include "sprite_asset.hpp"

#include <scarlett/scene.hpp>

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

// Reconciles live SceneState objects with cached sprite assets and draws them.
// It owns no script state: sync() only mirrors what the runtime currently has.
class SceneRenderer {
public:
	explicit SceneRenderer(const AssetLoader& loader)
		: _library(loader) {}

	void sync(const asb::SceneState& scene);
	void advance(uint32_t frames);
	void render(const glm::mat4& projection, const Camera& camera);
	void clear();

	size_t object_count() const { return _objects.size(); }
	AssetLibrary& assets() { return _library; }

private:
	struct Object {
		asb::RuntimeEntity state;
		SpriteAssetPtr asset;
		std::vector<SpriteInstance> instances;
		int track_index = -1;
		bool background = false;
	};

	static int choose_track(const SpriteAsset& asset, const asb::RuntimeEntity& e);
	void rebuild(Object& object, const asb::RuntimeEntity& e, SpriteAssetPtr asset);
	void apply(Object& object, const asb::RuntimeEntity& e);
	glm::vec2 parent_position(const Object& object) const;

	AssetLibrary _library;
	std::unordered_map<std::string, Object> _objects;
};
