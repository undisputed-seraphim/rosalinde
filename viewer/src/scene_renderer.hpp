#pragma once

#include "asset_library.hpp"
#include "camera.hpp"
#include "mpd_asset.hpp"
#include "sprite_asset.hpp"
#include "sprite_playback.hpp"
#include "stage_asset.hpp"
#include "weather_renderer.hpp"

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
	void set_stage(const StageComposition& composition);
	void set_weather_request(float request) { _weather_request = glm::clamp(request, 0.0f, 1.0f); }
	void set_trace(asb::TraceCallback fn);
	void set_filters(std::vector<std::string> only, std::vector<std::string> hide, bool keep_stage);
	void set_probe(ProbeWriter* probe) { _probe = probe; }
	void advance(float dt);
	void render(const glm::mat4& projection, const Camera& camera);
	void render_weather(const glm::mat4& projection, const Camera& camera);
	void clear();
	std::string state_json(const asb::SceneState& scene) const;
	std::string timeline_json(const asb::SceneState& scene) const;
	bool stage_replaced() const;
	std::string summary(const asb::SceneState& scene) const;

	size_t object_count() const { return _objects.size(); }

private:
	struct StageMesh {
		StageMpdLayer spec;
		std::unique_ptr<MpdAsset> asset;
		bool attempted = false;
	};

	struct WeatherRegion {
		StageLayer spec;
		glm::vec4 bounds{};
		bool rain = false;
	};

	struct RenderItem {
		RenderItem() = default;
		RenderItem(const RenderItem&) = delete;
		RenderItem& operator=(const RenderItem&) = delete;
		RenderItem(RenderItem&&) = default;
		RenderItem& operator=(RenderItem&&) = default;

		asb::RuntimeEntity state;
		SpritePlayback playback;
		std::vector<StageMesh> meshes;
		std::map<uint32_t, glm::vec4> tints;
		int tint_key = -2;
		bool background = false;
		bool stage = false;
		bool stage_layer = false;
		bool replaces_stage = false;
		std::vector<uint32_t> track_order;

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
	std::vector<StageLayer> _stage_layer_specs;
	std::vector<RenderItem> _stage_layer_items;
	std::vector<WeatherRegion> _weather_regions;
	WeatherRenderer _weather;
	float _weather_amount = 0.0f;
	float _weather_fog = 0.0f;
	float _weather_request = 0.0f;
	bool _stage_layers_built = false;

	std::string _stage_mbs;
	std::string _stage_ftx;
	std::vector<std::string> _only;
	std::vector<std::string> _hide;
	bool _keep_stage = false;
	ProbeWriter* _probe = nullptr;
};
