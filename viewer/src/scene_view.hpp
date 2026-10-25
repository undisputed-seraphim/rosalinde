#pragma once

#include "asset_loader.hpp"
#include "asset_library.hpp"
#include "camera.hpp"
#include "effect_manager.hpp"
#include "field_catalog.hpp"
#include "playback_clock.hpp"
#include "scene_renderer.hpp"
#include "screen_fade.hpp"
#include "stage_composer.hpp"
#include "viewer_config.hpp"

#include <glm/glm.hpp>
#include <string>

class ProbeWriter;

class SceneView {
public:
	SceneView(const AssetLoader& loader, AssetLibrary& library, const ViewerConfig& config);

	void initialize(const asb::SceneState& scene);
	void sync(const asb::SceneState& scene);
	void advance(float dt);
	void render(const asb::SceneState& scene, ProbeWriter* probe);

	void set_trace(asb::TraceCallback trace);
	void set_filters(std::vector<std::string> only, std::vector<std::string> hide, bool keep_stage);

	SceneRenderer& renderer() { return _renderer; }
	const SceneRenderer& renderer() const { return _renderer; }

private:
	void sync_stage(const asb::SceneState& scene);
	void apply_camera(const asb::SceneState& scene);
	void draw_text(const asb::SceneState& scene);

	const AssetLoader& _loader;
	std::string _entry;
	FieldCatalog _fields;
	StageComposer _stages;
	SceneRenderer _renderer;
	EffectManager _effects;
	Camera _camera;
	PlaybackClock _clock;
	glm::mat4 _projection;
	ScreenFadeOverlay _fade;
	asb::TraceCallback _trace;
	int _initial_stage = -1;
	int _stage_type = -1;
	int _stage_coa = -1;
	int _stage_weather = -1;
};
