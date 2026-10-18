#pragma once

#include "asset_library.hpp"
#include "camera.hpp"
#include "sprite_asset.hpp"

#include <scarlett/scene.hpp>
#include <scarlett/trace.hpp>

#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class ProbeWriter;

// Reconciles live SceneState objects with cached sprite assets and draws them.
// It owns no script state: sync() only mirrors what the runtime currently has.
class SceneRenderer {
public:
	explicit SceneRenderer(const AssetLoader& loader)
		: _library(loader) {}

	void sync(const asb::SceneState& scene);
	void set_stage(const std::string& mbs, const std::string& ftx);
	void set_trace(asb::TraceCallback fn);
	void set_filters(std::vector<std::string> only, std::vector<std::string> hide, bool keep_stage);
	void set_probe(ProbeWriter* probe) { _probe = probe; }
	void advance(uint32_t frames);
	void render(const glm::mat4& projection, const Camera& camera);
	void clear();
	std::string state_json(const asb::SceneState& scene) const;
	std::string summary(const asb::SceneState& scene) const;

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
	std::string filter_reason(const asb::RuntimeEntity& e, bool stage) const;
	std::string object_json(const Object& object, bool stage, std::string_view reason) const;
	void probe_object(const Object& object, bool stage, std::string_view reason);
	void probe_instance(const Object& object, bool stage, size_t index);
	void probe_layers(const Object& object, size_t index, bool stage, const std::map<uint32_t, glm::vec4>& tints);

	AssetLibrary _library;
	asb::TraceCallback _trace;
	long _frame = 0;
	std::unordered_map<std::string, Object> _objects;
	Object _stage;
	std::string _stage_mbs;
	std::string _stage_ftx;
	std::vector<std::string> _only;
	std::vector<std::string> _hide;
	bool _keep_stage = false;
	ProbeWriter* _probe = nullptr;
};
