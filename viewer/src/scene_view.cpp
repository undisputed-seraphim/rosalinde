#include "scene_view.hpp"

#include <glm/ext.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <utility>

SceneView::SceneView(const AssetLoader& loader, AssetLibrary& library, const ViewerConfig& config)
	: _loader(loader)
	, _entry(config.script)
	, _fields(loader, config.data_dir)
	, _renderer(library)
	, _effects(library)
	, _camera(1.0f)
	, _projection(glm::ortho(-960.0f, 960.0f, 540.0f, -540.0f)) {
	_initial_stage = config.initial_stage;
	if (const char* ortho = std::getenv("VIEWER_ORTHO")) {
		float a = 0, b = 0, c = 0, d = 0;
		if (std::sscanf(ortho, "%f,%f,%f,%f", &a, &b, &c, &d) == 4)
			_projection = glm::ortho(a, b, c, d);
	}
}

void SceneView::initialize(const asb::SceneState& scene) {
	if (_initial_stage >= 0) {
		auto seed = _fields.background(_initial_stage, 0, _entry);
		if (seed) {
			_stages.seed(std::move(*seed));
			_renderer.set_stage(_stages.current());
			spdlog::debug(
				"stage: seeded initial type={} mbs={} layers={} mpd={}",
				_initial_stage,
				_stages.current().mbs.empty() ? "<none>" : _stages.current().mbs,
				_stages.current().layers.size(),
				_stages.current().mpd_layers.size());
		}
	}
	sync_stage(scene);
	_renderer.sync(scene);
	_effects.update(scene.frame, scene.effects, [&scene](const std::string& name) -> glm::vec2 {
		if (const auto* entity = scene.find_entity(name))
			return {entity->pos[0], entity->pos[1]};
		return {0.0f, 0.0f};
	});
}

void SceneView::sync(const asb::SceneState& scene) {
	sync_stage(scene);
	_renderer.sync(scene);
	_effects.update(scene.frame, scene.effects, [&scene](const std::string& name) -> glm::vec2 {
		if (const auto* entity = scene.find_entity(name))
			return {entity->pos[0], entity->pos[1]};
		return {0.0f, 0.0f};
	});
}

void SceneView::advance(float dt) {
	_clock.advance(dt);
	_renderer.advance(dt);
	_effects.advance(dt);
}

void SceneView::sync_stage(const asb::SceneState& scene) {
	const int type = scene.stage.stage_type;
	const int coa = scene.stage.coa_preset;
	if (type == _stage_type && coa == _stage_coa)
		return;
	_stage_type = type;
	_stage_coa = coa;
	auto composition = _fields.background(type, coa, _entry);
	if (!composition) {
		spdlog::debug("stage: frame={} type={} -> none", scene.frame, type);
		_stages.clear();
		_renderer.set_stage(StageComposition{});
		return;
	}
	_stages.apply(std::move(*composition));
	const auto& stage = _stages.current();
	// TODO(stage): UcStStage +0xab0 (stage_data) should provide the initial
	// base composition and the replace/overlay decision for this stage.
	spdlog::debug(
		"stage: frame={} type={} -> mbs={} layers={} mpd={}",
		scene.frame,
		type,
		stage.mbs.empty() ? "<none>" : stage.mbs,
		stage.layers.size(),
		stage.mpd_layers.size());
	_renderer.set_stage(stage);
	if (_trace)
		_trace(
			{scene.frame,
			 "stage",
			 "asset",
			 "\"type\":" + asb::trace::integer(type) + ",\"mbs\":\"" + asb::trace::quote(stage.mbs) +
				 "\",\"ftx\":\"" + asb::trace::quote(stage.ftx) + "\",\"layers\":" +
				 std::to_string(stage.layers.size()) + ",\"mpd\":" + std::to_string(stage.mpd_layers.size())});
}

void SceneView::apply_camera(const asb::SceneState& scene) {
	const auto& cam = scene.camera;
	const float zoom = cam.has_zoom && cam.zoom > 0.0f ? cam.zoom : 1.0f;
	if (cam.has_pos) {
		float offset_y = 0.0f;
		if (const char* value = std::getenv("VIEWER_CAM_Y"))
			offset_y = static_cast<float>(std::atof(value));
		_camera.recenter({cam.pos[0], cam.pos[1] + offset_y});
	}
	if (cam.has_zoom && cam.zoom > 0.0f)
		_camera.set_zoom(cam.zoom);
	if (_trace)
		_trace(
			{scene.frame,
			 "camera",
			 "eval",
			 "\"pos\":" + asb::trace::vec3(cam.pos) + ",\"zoom\":" + asb::trace::number(zoom) +
				 ",\"origin\":" + asb::trace::vec3(cam.origin)});
}

void SceneView::draw_text(const asb::SceneState& scene) {
	if (scene.texts.empty())
		return;
	auto* draw = ImGui::GetForegroundDrawList();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	float y = viewport->WorkPos.y + viewport->WorkSize.y - 140.0f;
	for (const auto& text : scene.texts) {
		if (scene.frame < text.frame)
			continue;
		if (text.end > 0 && scene.frame >= text.end)
			continue;
		if (text.text.empty())
			continue;
		std::string line;
		if (!text.speaker.empty())
			line = text.speaker + ": ";
		line += text.text;
		draw->AddText(ImVec2(viewport->WorkPos.x + 40.0f, y), IM_COL32(255, 255, 255, 255), line.c_str());
		y += 28.0f;
	}
}

void SceneView::render(const asb::SceneState& scene, ProbeWriter* probe) {
	_renderer.set_probe(probe);
	apply_camera(scene);
	_renderer.render(_projection, _camera);
	_effects.draw(_projection, _camera, {0.0f, 0.0f});

	float fade[4];
	asb::fade_at(scene.fades, scene.frame, fade);
	_fade.draw(glm::vec4(fade[0], fade[1], fade[2], fade[3]));
	draw_text(scene);
	_renderer.set_probe(nullptr);
}

void SceneView::set_trace(asb::TraceCallback trace) {
	_trace = std::move(trace);
	_renderer.set_trace(_trace);
}

void SceneView::set_filters(std::vector<std::string> only, std::vector<std::string> hide, bool keep_stage) {
	_renderer.set_filters(std::move(only), std::move(hide), keep_stage);
}
