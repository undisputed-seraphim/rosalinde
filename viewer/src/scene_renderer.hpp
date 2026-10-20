#pragma once

#include "asset_library.hpp"
#include "camera.hpp"
#include "mpd_asset.hpp"
#include "sprite_asset.hpp"
#include "sprite_playback.hpp"
#include "stage_asset.hpp"

#include <scarlett/scene.hpp>
#include <scarlett/trace.hpp>

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class ProbeWriter;

// Reconciles live SceneState objects with cached sprite assets and draws them.
// It owns no script state: sync() only mirrors what the runtime currently has.
class SceneRenderer {
public:
	explicit SceneRenderer(AssetLibrary& library)
		: _library(library) {}

	void sync(const asb::SceneState& scene);
	void set_stage(const std::string& mbs, const std::string& ftx, std::vector<StageLayer> layers = {});
	void set_trace(asb::TraceCallback fn);
	void set_filters(std::vector<std::string> only, std::vector<std::string> hide, bool keep_stage);
	void set_probe(ProbeWriter* probe) { _probe = probe; }
	void advance(float dt);
	void render(const glm::mat4& projection, const Camera& camera);
	void clear();
	std::string state_json(const asb::SceneState& scene) const;
	std::string timeline_json(const asb::SceneState& scene) const;
	bool stage_replaced() const;
	std::string summary(const asb::SceneState& scene) const;

	size_t object_count() const { return _objects.size(); }

private:
	struct StageMesh {
		StageLayer spec;
		std::unique_ptr<MpdAsset> asset;
		bool attempted = false;
	};

	struct RenderItem {
		asb::RuntimeEntity state;
		SpritePlayback playback;
		std::vector<StageMesh> meshes;
		std::map<uint32_t, glm::vec4> tints;
		int tint_key = -2;
		bool background = false;
		bool stage = false;
		bool replaces_stage = false;

		bool has_renderable() const { return playback.asset != nullptr || !meshes.empty(); }
	};

	void rebuild(RenderItem& item, const asb::RuntimeEntity& e, SpriteAssetPtr asset);
	void apply(RenderItem& item, const asb::RuntimeEntity& e);
	void update_tints(RenderItem& item);
	glm::vec2 parent_position(const RenderItem& item) const;
	std::string filter_reason(const asb::RuntimeEntity& e, bool stage) const;
	std::string object_json(const RenderItem& item, bool stage, std::string_view reason) const;
	void probe_object(const RenderItem& item, bool stage, std::string_view reason);
	void probe_instance(const RenderItem& item, bool stage, size_t index);
	void probe_layers(const RenderItem& item, size_t index, bool stage, const std::map<uint32_t, glm::vec4>& tints);
	std::vector<const RenderItem*> ordered_items() const;
	std::vector<const RenderItem*> draw_items() const;

	AssetLibrary& _library;
	asb::TraceCallback _trace;
	long _frame = 0;
	std::unordered_map<std::string, RenderItem> _objects;
	RenderItem _stage;

	std::string _stage_mbs;
	std::string _stage_ftx;
	std::vector<std::string> _only;
	std::vector<std::string> _hide;
	bool _keep_stage = false;
	ProbeWriter* _probe = nullptr;
};
