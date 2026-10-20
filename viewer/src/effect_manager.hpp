#pragma once

#include "asset_library.hpp"
#include "camera.hpp"
#include "sprite_asset.hpp"

#include <scarlett/scene.hpp>

#include <functional>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

// Plays the `Timeline.effects` stream: DrCreateEffect spawns an MBS/FTX effect
// instance, then direction/position/move/rotate/scale/attach events mutate it.
// A SpriteData + SpriteRenderer is shared per (mbs,ftx); each effect owns only
// its sprite instance. Point lights are handled separately.
class EffectManager {
public:
	using EntityPos = std::function<glm::vec2(const std::string&)>;

	explicit EffectManager(AssetLibrary& library) : _library(library) {}

	void reset();
	void update(long frame, const std::vector<asb::RuntimeEffect>& events, const EntityPos& entity_pos);
	void advance(float dt);
	void draw(const glm::mat4& projection, const Camera& cam, const glm::vec2& shake);

	size_t size() const { return _effects.size(); }

private:
	struct Effect {
		std::string name;
		SpriteAssetPtr asset;
		SpriteInstance instance;
		glm::vec2 base_pos{0.0f, 0.0f};
		glm::vec2 offset{0.0f, 0.0f};
		glm::vec2 pos{0.0f, 0.0f};
		glm::vec2 move_from{0.0f, 0.0f}, move_to{0.0f, 0.0f};
		float scale = 1.0f, scale_from = 1.0f, scale_to = 1.0f;
		float rot = 0.0f, rot_from = 0.0f, rot_to = 0.0f;
		float alpha = 1.0f;
		float dir_x = 1.0f;
		long start = 0, end = 0;
		long fade_in = 0;
		long move_f0 = 0, move_f1 = 0;
		long scale_f0 = 0, scale_f1 = 0;
		long rot_f0 = 0, rot_f1 = 0;
		bool moving = false, scaling = false, rotating = false;
		bool auto_delete = false;
		std::string attached;
	};

	void apply(const asb::RuntimeEffect& e, long frame, const EntityPos& entity_pos);
	SpriteAssetPtr asset_for(const std::string& mbs, const std::string& ftx);
	static long track_frames(const SpriteData& data, int track);

	AssetLibrary& _library;
	std::unordered_map<std::string, Effect> _effects;
	size_t _next = 0;
};
