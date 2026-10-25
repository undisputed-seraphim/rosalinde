#include "scene_renderer.hpp"
#include "probe.hpp"
#include "scene_util.hpp"

#include <glm/ext.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <sstream>
#include <string_view>

namespace {

bool flip_v_for(const asb::RuntimeEntity& e) {
	const char* match = std::getenv("VIEWER_FLIP_V");
	if (!match || !*match)
		return false;
	const std::string_view value(match);
	if (value == "1" || value == "all")
		return true;
	return e.mbs.find(value) != std::string::npos || e.ftx.find(value) != std::string::npos;
}

bool name_matches(const asb::RuntimeEntity& e, std::string_view name) {
	if (e.name == name)
		return true;
	for (const auto& alias : e.aliases)
		if (alias == name)
			return true;
	return false;
}

bool name_contains(const asb::RuntimeEntity& e, std::string_view text) {
	if (e.name.find(text) != std::string::npos)
		return true;
	for (const auto& alias : e.aliases)
		if (alias.find(text) != std::string::npos)
			return true;
	return false;
}

std::string quote(std::string_view value) { return "\"" + asb::trace::quote(value) + "\""; }

std::string number(double value) {
	if (!std::isfinite(value))
		return "null";
	return asb::trace::number(value);
}

std::string vec2_json(const glm::vec2& v) { return "[" + number(v.x) + "," + number(v.y) + "]"; }

std::string vec3_json(const glm::vec3& v) { return "[" + number(v.x) + "," + number(v.y) + "," + number(v.z) + "]"; }

std::string vec4_json(const glm::vec4& v) {
	if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z) || !std::isfinite(v.w))
		return "null";
	return "[" + number(v.x) + "," + number(v.y) + "," + number(v.z) + "," + number(v.w) + "]";
}

std::string color_json(const uint32_t color[4]) {
	return "[" + std::to_string(color[0]) + "," + std::to_string(color[1]) + "," + std::to_string(color[2]) + "," +
		   std::to_string(color[3]) + "]";
}

std::string runs_json(const SpriteInstance& instance) {
	std::string out = "[";
	const auto states = instance.run_states();
	for (size_t i = 0; i < states.size(); ++i) {
		if (i)
			out += ',';
		const auto& state = states[i];
		uint32_t track_slot = 0;
		if (instance.data && instance.playback.track_idx < instance.data->tracks.size() &&
			i < instance.data->tracks[instance.playback.track_idx].runs.size())
			track_slot = instance.data->tracks[instance.playback.track_idx].runs[i].sa_track_slot;
		out += "{\"run\":" + std::to_string(i) + ",\"track_id\":" + std::to_string(track_slot) +
			   ",\"offset\":" + std::to_string(state.offset) + ",\"s8\":" + std::to_string(state.s8) +
			   ",\"s6\":" + std::to_string(state.s6) + ",\"s7\":" + std::to_string(state.s7) +
			   ",\"flags\":" + std::to_string(state.flags) + ",\"frames\":" + std::to_string(state.frames) +
			   ",\"done\":" + asb::trace::boolean(state.done) + ",\"hidden\":" + asb::trace::boolean(state.hidden) +
			   "}";
	}
	out += ']';
	return out;
}

asb::RuntimeEntity faded(const asb::RuntimeEntity& e, uint32_t frame) {
	asb::RuntimeEntity view = e;
	if (view.fade_duration <= 0 || frame < static_cast<uint32_t>(view.fade_start))
		return view;
	const float t =
		std::clamp(static_cast<float>(frame - view.fade_start) / static_cast<float>(view.fade_duration), 0.0f, 1.0f);
	view.alpha = static_cast<int>(std::lround(view.fade_from + (view.fade_to - view.fade_from) * t));
	return view;
}

} // namespace

bool SceneRenderer::stage_replaced() const {
	for (const auto& [_, object] : _objects)
		if (object.replaces_stage)
			return true;
	return false;
}

void SceneRenderer::set_trace(asb::TraceCallback fn) {
	_trace = std::move(fn);
	_library.set_trace(_trace);
}

void SceneRenderer::set_filters(std::vector<std::string> only, std::vector<std::string> hide, bool keep_stage) {
	_only = std::move(only);
	_hide = std::move(hide);
	_keep_stage = keep_stage;
}

std::string SceneRenderer::filter_reason(const asb::RuntimeEntity& e, bool stage) const {
	if (stage) {
		if (!_only.empty() && !_keep_stage)
			return "stage";
	} else if (!_only.empty()) {
		bool matched = false;
		for (const auto& name : _only)
			matched = matched || name_matches(e, name);
		if (!matched)
			return "only";
	}
	for (const auto& text : _hide)
		if (name_contains(e, text))
			return "hide";
	if (const char* text = std::getenv("VIEWER_HIDE"); text && name_contains(e, text))
		return "viewer_hide";
	return {};
}

void SceneRenderer::reset_stage() {
	_stage_layer_specs.clear();
	_stage_layer_items.clear();
	_weather_regions.clear();
	_stage_layers_built = false;
	_stage = RenderItem{};
	_stage_mbs.clear();
	_stage_ftx.clear();
}

void SceneRenderer::set_stage(const StageComposition& composition) {
	reset_stage();
	_stage_layer_specs = composition.layers;
	_weather_amount = composition.weather_amount;
	_weather_fog = composition.weather_fog;
	_stage_mbs = composition.mbs;
	_stage_ftx = composition.ftx;

	_stage.stage = true;
	_stage.state.visible = true;
	_stage.state.order = -100000;
	_stage.state.alpha = 255;
	_stage.state.space = _stage_mbs.rfind("FieldBG/", 0) == 0 || !composition.mpd_layers.empty()
							 ? asb::RuntimeEntity::Space::Ui
							 : asb::RuntimeEntity::Space::World;
	_stage.meshes.reserve(composition.mpd_layers.size());
	for (const auto& layer : composition.mpd_layers)
		_stage.meshes.push_back({layer, nullptr, false});
}

void SceneRenderer::rebuild(RenderItem& object, const asb::RuntimeEntity& e, SpriteAssetPtr asset) {
	object.state = e;
	object.background = asset && asset->is_background;
	object.replaces_stage = object.background && e.started && e.visible && e.alpha > 0;
	object.tint_key = -2;
	const TrackPolicy policy =
		object.background && !object.stage_layer ? TrackPolicy::AllEnabledTracks : TrackPolicy::ScriptTrack;
	const bool character = e.kind == "DrUiCreateChara" || e.kind == "DrCreateChara" || e.kind == "DrCreateNpc";
	const bool stateful_mone = asset && e.kind.rfind("CreateSpriteMone", 0) == 0 && !asset->data.tracks.empty() &&
							   (e.track_index >= 0 && static_cast<size_t>(e.track_index) < asset->data.tracks.size()
									? asset->data.tracks[e.track_index].sa_main_sequence > 0
									: asset->data.tracks.front().sa_main_sequence > 0);
	const bool hold_finished = object.background || character || object.stage_layer || stateful_mone;
	object.playback.reset(std::move(asset), policy, e, flip_v_for(e), hold_finished, character);
	object.playback.apply_track_order(object.track_order);
	object.replaces_stage = object.replaces_stage && object.playback.asset && !object.playback.instances.empty();
	update_tints(object);

	if (object.background && object.playback.asset)
		spdlog::debug(
			"bg: {} tracks={} selected={}",
			e.mbs,
			object.playback.asset->data.tracks.size(),
			object.playback.instances.size());
	if (_trace)
		_trace(
			{_frame,
			 "sprite",
			 "rebuild",
			 "\"name\":\"" + asb::trace::quote(e.name) + "\",\"mbs\":\"" + asb::trace::quote(e.mbs) + "\",\"ftx\":\"" +
				 asb::trace::quote(e.ftx) + "\",\"background\":" + asb::trace::boolean(object.background) +
				 ",\"instances\":" + asb::trace::integer(static_cast<long>(object.playback.instances.size()))});
}

void SceneRenderer::apply(RenderItem& object, const asb::RuntimeEntity& e) {
	object.state = e;
	object.replaces_stage = object.background && e.started && e.visible && e.alpha > 0 && object.playback.asset &&
							!object.playback.instances.empty();
	const int previous_track = object.playback.track_index;
	object.playback.apply(e);
	update_tints(object);
	if (_trace && previous_track != object.playback.track_index && object.playback.track_index >= 0)
		_trace(
			{_frame,
			 "sprite",
			 "track",
			 "\"name\":\"" + asb::trace::quote(e.name) +
				 "\",\"track\":" + asb::trace::integer(object.playback.track_index)});
}

void SceneRenderer::update_tints(RenderItem& object) {
	const int packed = object.state.color[0] >= 0 ? object.state.color[0] : object.state.color[2];
	if (packed == object.tint_key)
		return;
	object.tint_key = packed;
	object.tints.clear();
	if (packed < 0 || !object.playback.asset)
		return;
	const glm::vec4 tint{
		((packed >> 16) & 0xFF) / 255.0f, ((packed >> 8) & 0xFF) / 255.0f, (packed & 0xFF) / 255.0f, 1.0f};
	object.tints[0] = tint;
	for (const auto& s4 : object.playback.asset->data.v77.s4)
		object.tints[s4.attributes] = tint;
}

void SceneRenderer::sync(const asb::SceneState& scene) {
	_frame = scene.frame;
	_library.set_frame(_frame);
	std::unordered_map<std::string, bool> seen;
	seen.reserve(scene.entities.size());

	for (const auto& e : scene.entities) {
		if (e.name.empty() || e.mbs.empty() || e.ftx.empty() || !e.visible || !e.started)
			continue;
		seen[e.name] = true;
		const auto view = faded(e, _frame);

		auto it = _objects.find(e.name);
		if (it != _objects.end()) {
			const bool same_asset = it->second.state.mbs == e.mbs && it->second.state.ftx == e.ftx;
			if (same_asset) {
				apply(it->second, view);
				continue;
			}
			_objects.erase(it);
		}

		auto asset = _library.get(e.mbs, e.ftx);
		if (!asset)
			continue;
		RenderItem object;
		rebuild(object, view, std::move(asset));
		_objects.emplace(e.name, std::move(object));
	}

	for (auto it = _objects.begin(); it != _objects.end();) {
		if (!seen.contains(it->first))
			it = _objects.erase(it);
		else
			++it;
	}

	if (!_stage_mbs.empty() && !_stage_ftx.empty() && (!_stage.playback.asset || _stage.state.mbs != _stage_mbs)) {
		auto asset = _library.get(_stage_mbs, _stage_ftx);
		if (asset) {
			asb::RuntimeEntity e;
			e.name = "__stage__";
			e.kind = "StageBG";
			e.mbs = _stage_mbs;
			e.ftx = _stage_ftx;
			e.space =
				_stage_mbs.rfind("FieldBG/", 0) == 0 ? asb::RuntimeEntity::Space::Ui : asb::RuntimeEntity::Space::World;
			e.order = -100000;
			e.alpha = 255;
			rebuild(_stage, e, std::move(asset));
		}
	}

	if (!_stage_layer_specs.empty() && !_stage_layers_built) {
		_stage_layers_built = true;
		_stage_layer_items.reserve(_stage_layer_specs.size());
		for (size_t i = 0; i < _stage_layer_specs.size(); ++i) {
			const auto& spec = _stage_layer_specs[i];
			auto asset = _library.get(spec.mbs, spec.ftx);
			if (!asset)
				continue;
			const SpriteData::Track* track =
				spec.track_index >= 0 && static_cast<size_t>(spec.track_index) < asset->data.tracks.size()
					? &asset->data.tracks[static_cast<size_t>(spec.track_index)]
					: nullptr;
			if (track && (track->name.find("RAIN_FOG") != std::string::npos ||
						  track->name.find("SNOW_FOG") != std::string::npos)) {
				WeatherRegion region;
				region.spec = spec;
				region.bounds = track->bounds;
				region.rain = track->name.find("RAIN") != std::string::npos;
				spdlog::trace(
					"weather: layer={} track={} name={} rain={} bounds=({},{},{},{})",
					i,
					spec.track_index,
					track->name,
					region.rain,
					track->bounds.x,
					track->bounds.y,
					track->bounds.z,
					track->bounds.w);
				_weather_regions.push_back(std::move(region));
				continue;
			}
			asb::RuntimeEntity e;
			e.name = "__stage__:" + std::to_string(i);
			e.kind = "StageLayer";
			e.mbs = spec.mbs;
			e.ftx = spec.ftx;
			e.track_index = spec.track_index;
			e.started = true;
			e.visible = true;
			e.space =
				spec.mbs.rfind("FieldBG/", 0) == 0 ? asb::RuntimeEntity::Space::Ui : asb::RuntimeEntity::Space::World;
			e.order = -100000 + spec.order;
			e.pos[0] = spec.x;
			e.pos[1] = spec.y;
			e.pos[2] = spec.z;
			e.scale = spec.scale;
			e.scale_y = spec.scale_y;
			e.alpha = spec.alpha;
			RenderItem object;
			object.stage = true;
			object.stage_layer = true;
			rebuild(object, e, std::move(asset));
			_stage_layer_items.push_back(std::move(object));
		}
		spdlog::debug(
			"weather: stage amount={} request={} fog={} regions={}",
			_weather_amount,
			_weather_request,
			_weather_fog,
			_weather_regions.size());
	}

	for (auto& layer : _stage.meshes) {
		if (layer.attempted)
			continue;
		layer.attempted = true;
		layer.asset = MpdAsset::load(_library.loader(), layer.spec.mpd, layer.spec.ftx);
		if (!layer.asset)
			spdlog::warn("stage: failed to load MPD {} ({})", layer.spec.mpd, layer.spec.ftx);
	}
}

void SceneRenderer::advance(float dt) {
	if (dt <= 0.0f)
		return;
	for (auto& [_, object] : _objects)
		object.playback.advance(dt);
	for (auto& object : _stage_layer_items)
		object.playback.advance(dt);
	_stage.playback.advance(dt);
}

glm::vec2 SceneRenderer::parent_position(const RenderItem& object) const {
	if (object.state.parent.empty())
		return {0.0f, 0.0f};
	auto it = _objects.find(object.state.parent);
	if (it == _objects.end())
		return {0.0f, 0.0f};
	return {it->second.state.pos[0], it->second.state.pos[1]};
}

std::vector<const SceneRenderer::RenderItem*> SceneRenderer::ordered_items() const {
	std::vector<const RenderItem*> items;
	items.reserve(_objects.size() + _stage_layer_items.size() + 1);
	for (const auto& [_, object] : _objects)
		items.push_back(&object);
	for (const auto& object : _stage_layer_items)
		if (object.has_renderable())
			items.push_back(&object);
	if (_stage.has_renderable())
		items.push_back(&_stage);
	std::stable_sort(items.begin(), items.end(), [](const RenderItem* a, const RenderItem* b) {
		const auto& ea = a->state;
		const auto& eb = b->state;
		if (ea.prerender_force != eb.prerender_force)
			return ea.prerender_force > eb.prerender_force;
		if (ea.window_id_offset != eb.window_id_offset)
			return ea.window_id_offset < eb.window_id_offset;
		if (ea.dlprio != eb.dlprio)
			return ea.dlprio < eb.dlprio;
		if (ea.sort_id_offset != eb.sort_id_offset)
			return ea.sort_id_offset < eb.sort_id_offset;
		return ea.order < eb.order;
	});
	return items;
}

std::vector<const SceneRenderer::RenderItem*> SceneRenderer::draw_items() const {
	auto items = ordered_items();
	const bool replaced = stage_replaced();
	items.erase(
		std::remove_if(
			items.begin(),
			items.end(),
			[&](const RenderItem* item) {
				if (!item->state.visible || !item->has_renderable())
					return true;
				if (item->stage)
					return replaced || (item->playback.instances.empty() && item->meshes.empty());
				return item->playback.instances.empty();
			}),
		items.end());
	return items;
}

std::string SceneRenderer::object_json(const RenderItem& object, bool stage, std::string_view reason) const {
	const auto& e = object.state;
	std::string out =
		"{\"name\":" + quote(e.name) + ",\"stage\":" + asb::trace::boolean(stage) + ",\"kind\":" + quote(e.kind) +
		",\"mbs\":" + quote(e.mbs) + ",\"ftx\":" + quote(e.ftx) +
		",\"space\":" + quote(e.space == asb::RuntimeEntity::Space::Ui ? "ui" : "world") +
		",\"order\":" + std::to_string(e.order) + ",\"sort_id_offset\":" + std::to_string(e.sort_id_offset) +
		",\"dlprio\":" + std::to_string(e.dlprio) + ",\"window_id_offset\":" + std::to_string(e.window_id_offset) +
		",\"prerender_force\":" + asb::trace::boolean(e.prerender_force) + ",\"alpha\":" + std::to_string(e.alpha) +
		",\"visible\":" + asb::trace::boolean(e.visible) + ",\"started\":" + asb::trace::boolean(e.started) +
		",\"scale\":" + number(e.scale) + ",\"pos\":[" + number(e.pos[0]) + "," + number(e.pos[1]) + "," +
		number(e.pos[2]) + "]" + ",\"parent\":" + quote(e.parent) + ",\"layer\":" + std::to_string(e.layer) +
		",\"layer_mask\":" + std::to_string(e.layer_mask) + ",\"color\":[" + std::to_string(e.color[0]) + "," +
		std::to_string(e.color[1]) + "," + std::to_string(e.color[2]) + "," + std::to_string(e.color[3]) + "]" +
		",\"paused\":" + asb::trace::boolean(e.paused) + ",\"anim_paused\":" + asb::trace::boolean(e.anim_paused) +
		",\"animation_start_frame\":" + std::to_string(e.animation_start_frame) +
		",\"idle_track\":" + quote(e.idle_track) + ",\"idle_track_index\":" + std::to_string(e.idle_track_index) +
		",\"action_active\":" + asb::trace::boolean(e.action_active) +
		",\"action_end_frame\":" + std::to_string(e.action_end_frame) +
		",\"background\":" + asb::trace::boolean(object.background) +
		",\"replaces_stage\":" + asb::trace::boolean(object.replaces_stage) +
		",\"asset_loaded\":" + asb::trace::boolean(object.playback.asset != nullptr);

	std::string track_name;
	if (object.playback.asset && object.playback.track_index >= 0 &&
		static_cast<size_t>(object.playback.track_index) < object.playback.asset->data.tracks.size())
		track_name = object.playback.asset->data.tracks[object.playback.track_index].name;
	out += ",\"track_index\":" + std::to_string(object.playback.track_index) + ",\"track_name\":" + quote(track_name);

	out += ",\"instances\":[";
	for (size_t i = 0; i < object.playback.instances.size(); ++i) {
		const auto& instance = object.playback.instances[i];
		if (i)
			out += ',';
		std::string instance_track;
		if (instance.data && instance.playback.track_idx < instance.data->tracks.size())
			instance_track = instance.data->tracks[instance.playback.track_idx].name;
		out += "{\"index\":" + std::to_string(i) + ",\"track\":" + std::to_string(instance.playback.track_idx) +
			   ",\"track_name\":" + quote(instance_track) +
			   ",\"attribute_mask\":" + std::to_string(instance.attribute_mask) +
			   ",\"frame_counter\":" + std::to_string(instance.frame_counter()) +
			   ",\"hold_finished\":" + asb::trace::boolean(instance.playback.hold_finished) +
			   ",\"bounds\":" + vec4_json(instance.render_bounds()) + ",\"runs\":" + runs_json(instance) + "}";
	}
	out += "]";

	glm::vec4 bounds{
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity()};
	for (const auto& instance : object.playback.instances) {
		const auto b = instance.render_bounds();
		bounds.x = std::min(bounds.x, b.x);
		bounds.y = std::min(bounds.y, b.y);
		bounds.z = std::max(bounds.z, b.z);
		bounds.w = std::max(bounds.w, b.w);
	}
	out += ",\"bounds\":" + vec4_json(bounds) + ",\"reason\":" + quote(reason) + "}";
	return out;
}

void SceneRenderer::probe_object(const RenderItem& object, bool stage, std::string_view reason) {
	if (!_probe)
		return;
	_probe->draw() << "{\"frame\":" << _frame
				   << ",\"kind\":\"draw\",\"event\":\"object\",\"object\":" << object_json(object, stage, reason)
				   << "}\n";
}

void SceneRenderer::probe_instance(const RenderItem& object, bool stage, size_t index) {
	if (!_probe || index >= object.playback.instances.size())
		return;
	const auto& instance = object.playback.instances[index];
	std::string track_name;
	if (instance.data && instance.playback.track_idx < instance.data->tracks.size())
		track_name = instance.data->tracks[instance.playback.track_idx].name;
	_probe->draw() << "{\"frame\":" << _frame
				   << ",\"kind\":\"draw\",\"event\":\"instance\",\"object\":" << quote(object.state.name)
				   << ",\"stage\":" << asb::trace::boolean(stage) << ",\"instance\":" << index
				   << ",\"track\":" << instance.playback.track_idx << ",\"track_name\":" << quote(track_name)
				   << ",\"attribute_mask\":" << instance.attribute_mask
				   << ",\"frame_counter\":" << instance.frame_counter()
				   << ",\"bounds\":" << vec4_json(instance.render_bounds()) << ",\"runs\":" << runs_json(instance)
				   << "}\n";
}

void SceneRenderer::probe_layers(
	const RenderItem& object,
	size_t index,
	bool stage,
	const std::map<uint32_t, glm::vec4>& tints) {
	if (!_probe || index >= object.playback.instances.size())
		return;
	const auto& instance = object.playback.instances[index];
	if (!instance.data || instance.playback.track_idx >= instance.data->tracks.size())
		return;
	for (uint32_t run = 0; run < instance.sa_count(); ++run) {
		instance.visit_layers(
			run, tints.empty() ? nullptr : &tints, true, object.state.layer_mask, [&](const LayerVisit& layer) {
				std::string uv = "[";
				std::string uv_norm = "[";
				for (int k = 0; k < 4; ++k) {
					if (k) {
						uv += ',';
						uv_norm += ',';
					}
					uv += vec2_json(layer.uv[k]);
					if (layer.tex_width && layer.tex_height) {
						uv_norm += "[" + number(layer.uv[k].x / layer.tex_width) + "," +
								   number(layer.uv[k].y / layer.tex_height) + "]";
					} else {
						uv_norm += "[0,0]";
					}
				}
				uv += ']';
				uv_norm += ']';

				std::string xy = "[";
				for (int k = 0; k < 4; ++k) {
					if (k)
						xy += ',';
					xy += vec2_json(layer.xy[k]);
				}
				xy += ']';

				_probe->draw()
					<< "{\"frame\":" << _frame
					<< ",\"kind\":\"draw\",\"event\":\"layer\",\"object\":" << quote(object.state.name)
					<< ",\"stage\":" << asb::trace::boolean(stage) << ",\"instance\":" << index
					<< ",\"run\":" << layer.run << ",\"layer\":" << layer.layer
					<< ",\"track\":" << instance.playback.track_idx << ",\"track_id\":" << layer.sa_track_slot
					<< ",\"offset\":" << layer.offset << ",\"done\":" << asb::trace::boolean(layer.done)
					<< ",\"hidden\":" << asb::trace::boolean(layer.hidden) << ",\"s8\":" << layer.s8_frame_start
					<< ",\"s8_flags\":" << layer.s8_flags << ",\"s8_frames\":" << layer.s8_frames
					<< ",\"s6\":" << layer.s6_keyframe_id << ",\"s7\":" << layer.s7_transform_id
					<< ",\"s6_interp\":" << static_cast<unsigned>(layer.s6_keyframe_interpolation)
					<< ",\"s7_interp\":" << static_cast<unsigned>(layer.s7_transform_interpolation)
					<< ",\"s4\":" << layer.s4_layer_start << ",\"s4_source_tile_id\":" << layer.s4_source_tile_id
					<< ",\"s4_flags\":" << static_cast<unsigned>(layer.s4_flags) << ",\"s1\":" << layer.s1_uv_id
					<< ",\"s2\":" << layer.s2_vertex_id << ",\"s0\":" << layer.s0_color_row_id
					<< ",\"color_id\":" << static_cast<unsigned>(layer.color_id)
					<< ",\"alt_color\":" << color_json(layer.alt_color) << ",\"tex\":" << layer.tex_id
					<< ",\"tex_w\":" << layer.tex_width << ",\"tex_h\":" << layer.tex_height
					<< ",\"blend\":" << static_cast<unsigned>(layer.blend) << ",\"attributes\":" << layer.attributes
					<< ",\"notex\":" << asb::trace::boolean(layer.notex)
					<< ",\"flip_v\":" << asb::trace::boolean(layer.flip_v)
					<< ",\"drawn\":" << asb::trace::boolean(layer.drawn) << ",\"skip\":" << quote(layer.skip)
					<< ",\"depth\":" << number(layer.depth) << ",\"transition_t\":" << number(layer.transition_t)
					<< ",\"s6_bounds\":" << vec4_json(layer.s6_bounds) << ",\"s7_move\":" << vec3_json(layer.s7_move)
					<< ",\"s7_rotate\":" << vec3_json(layer.s7_rotate) << ",\"s7_scale\":" << vec2_json(layer.s7_scale)
					<< ",\"s7_fog\":" << layer.s7_fog << ",\"s1_center\":" << vec2_json(layer.s1_center)
					<< ",\"s1_topleft\":" << vec2_json(layer.s1_topleft) << ",\"uv\":" << uv
					<< ",\"uv_norm\":" << uv_norm << ",\"xy\":" << xy
					<< ",\"raw_color\":" << color_json(layer.raw_color) << ",\"color\":" << color_json(layer.color)
					<< "}\n";
			});
	}
}

std::string SceneRenderer::state_json(const asb::SceneState& scene) const {
	const auto objects = ordered_items();

	std::string out = "{\"objects\":[";
	bool first = true;
	for (const RenderItem* object : objects) {
		if (!first)
			out += ',';
		first = false;
		const bool stage = object->stage;
		out += object_json(*object, stage, filter_reason(object->state, stage));
	}
	out += "],\"missing\":[";
	first = true;
	for (const auto& e : scene.entities) {
		if (e.mbs.empty() || e.ftx.empty())
			continue;
		if (_objects.contains(e.name))
			continue;
		if (!first)
			out += ',';
		first = false;
		const std::string reason = !e.started ? "not_started" : (!e.visible ? "invisible" : "asset");
		out += "{\"name\":" + quote(e.name) + ",\"mbs\":" + quote(e.mbs) + ",\"ftx\":" + quote(e.ftx) +
			   ",\"visible\":" + asb::trace::boolean(e.visible) + ",\"started\":" + asb::trace::boolean(e.started) +
			   ",\"reason\":" + quote(reason) + "}";
	}
	out += "]}";
	return out;
}

std::string SceneRenderer::timeline_json(const asb::SceneState& scene) const {
	auto objects = ordered_items();
	if (stage_replaced())
		objects.erase(
			std::remove_if(objects.begin(), objects.end(), [](const RenderItem* object) { return object->stage; }),
			objects.end());

	std::string out = "{\"scene_frame\":" + std::to_string(scene.frame) + ",\"objects\":[";
	bool first = true;
	for (const RenderItem* object : objects) {
		if (!first)
			out += ',';
		first = false;
		const auto& e = object->state;
		out += "{\"name\":" + quote(e.name) + ",\"kind\":" + quote(e.kind) + ",\"mbs\":" + quote(e.mbs) +
			   ",\"order\":" + std::to_string(e.order) + ",\"alpha\":" + std::to_string(e.alpha) +
			   ",\"visible\":" + asb::trace::boolean(e.visible) + ",\"started\":" + asb::trace::boolean(e.started) +
			   ",\"background\":" + asb::trace::boolean(object->background) +
			   ",\"track\":" + std::to_string(object->playback.track_index) + ",\"instances\":[";
		for (size_t i = 0; i < object->playback.instances.size(); ++i) {
			if (i)
				out += ',';
			const auto& instance = object->playback.instances[i];
			std::string track_name;
			if (instance.data && instance.playback.track_idx < instance.data->tracks.size())
				track_name = instance.data->tracks[instance.playback.track_idx].name;
			out += "{\"index\":" + std::to_string(i) + ",\"track\":" + std::to_string(instance.playback.track_idx) +
				   ",\"track_name\":" + quote(track_name) + ",\"runs\":" + runs_json(instance) + "}";
		}
		out += "]}";
	}
	out += "]}";
	return out;
}

std::string SceneRenderer::summary(const asb::SceneState& scene) const {
	std::ostringstream out;
	out << "frame: " << scene.frame << " entities: " << scene.entities.size() << " objects: " << _objects.size()
		<< '\n';
	for (const auto& e : scene.entities) {
		const auto it = _objects.find(e.name);
		const bool loaded = it != _objects.end();
		out << e.name << " kind=" << e.kind << " mbs=" << e.mbs << " ftx=" << e.ftx << " track=" << e.track
			<< " track_index=" << e.track_index << " order=" << e.order << " alpha=" << e.alpha
			<< " visible=" << (e.visible ? "true" : "false") << " started=" << (e.started ? "true" : "false")
			<< " object=" << (loaded ? "yes" : "no") << '\n';
	}
	out << "renderer:\n";
	const auto objects = ordered_items();
	for (const RenderItem* object : objects) {
		const bool stage = object->stage;
		std::string track_name;
		if (object->playback.asset && object->playback.track_index >= 0 &&
			static_cast<size_t>(object->playback.track_index) < object->playback.asset->data.tracks.size())
			track_name = object->playback.asset->data.tracks[object->playback.track_index].name;
		out << object->state.name << " track=" << object->playback.track_index << " name=" << track_name
			<< " instances=" << object->playback.instances.size() << " reason=" << filter_reason(object->state, stage)
			<< '\n';
	}
	return out.str();
}

void SceneRenderer::render(const glm::mat4& projection, const Camera& camera) {
	enable_blend(glm::vec4(1.0f));
	enable_depth(GL_ALWAYS);
	glClear(GL_DEPTH_BUFFER_BIT);

	static const Camera ui_camera;
	const auto order = draw_items();
	for (const RenderItem* object : order) {
		if (!object->stage)
			continue;
		const Camera& stage_camera = object->state.space == asb::RuntimeEntity::Space::Ui ? ui_camera : camera;
		for (const auto& layer : object->meshes)
			if (layer.asset)
				layer.asset->draw(projection, stage_camera);
	}

	if (_trace) {
		std::string objects;
		for (const RenderItem* object : order) {
			const auto& e = object->state;
			glm::vec4 bounds{
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity(),
				-std::numeric_limits<float>::infinity(),
				-std::numeric_limits<float>::infinity()};
			for (const auto& instance : object->playback.instances) {
				const auto b = instance.render_bounds();
				bounds.x = std::min(bounds.x, b.x);
				bounds.y = std::min(bounds.y, b.y);
				bounds.z = std::max(bounds.z, b.z);
				bounds.w = std::max(bounds.w, b.w);
			}
			std::string track_name;
			if (object->playback.asset && object->playback.track_index >= 0 &&
				static_cast<size_t>(object->playback.track_index) < object->playback.asset->data.tracks.size())
				track_name = object->playback.asset->data.tracks[object->playback.track_index].name;
			objects += (objects.empty() ? "" : ",");
			objects += "{\"name\":\"" + asb::trace::quote(e.name) + "\",\"order\":" + asb::trace::integer(e.order) +
					   ",\"instances\":" + asb::trace::integer(static_cast<long>(object->playback.instances.size())) +
					   ",\"track\":" + asb::trace::integer(object->playback.track_index) + ",\"track_name\":\"" +
					   asb::trace::quote(track_name) + "\"" +
					   ",\"prerender_force\":" + asb::trace::boolean(e.prerender_force) +
					   ",\"window_id_offset\":" + asb::trace::integer(e.window_id_offset) +
					   ",\"dlprio\":" + asb::trace::integer(e.dlprio) +
					   ",\"sort_id_offset\":" + asb::trace::integer(e.sort_id_offset) + ",\"space\":\"" +
					   (e.space == asb::RuntimeEntity::Space::Ui ? "ui" : "world") + "\"" +
					   ",\"pos\":" + asb::trace::vec2(e.pos) + ",\"scale\":" + asb::trace::number(e.scale) +
					   ",\"alpha\":" + asb::trace::integer(e.alpha) +
					   ",\"background\":" + asb::trace::boolean(object->background) + ",\"reason\":\"" +
					   filter_reason(e, object->stage) + "\"" + ",\"bounds\":" + asb::trace::vec4(&bounds.x) + "}";
		}
		_trace({_frame, "draw", "list", "\"objects\":[" + objects + "]"});
	}
	for (const RenderItem* object : order) {
		const auto& e = object->state;
		const glm::vec2 offset = parent_position(*object);
		const glm::vec2 pos{e.pos[0] + offset.x, e.pos[1] + offset.y};
		const float alpha = std::clamp(e.alpha / 255.0f, 0.0f, 1.0f);
		std::string reason = filter_reason(e, object->stage);
		if (reason.empty() && alpha <= 0.0f)
			reason = "alpha";
		if (_probe)
			probe_object(*object, object->stage, reason);
		if (!reason.empty())
			continue;
		const bool flip = (e.dir_x < 0.0f) != e.reverse_x;

		for (size_t i = 0; i < object->playback.instances.size(); ++i) {
			auto& instance = object->playback.instances[i];
			if (_trace)
				_trace(
					{_frame,
					 "draw",
					 "runs",
					 "\"object\":\"" + asb::trace::quote(e.name) + "\",\"instance\":" + std::to_string(i) +
						 ",\"track\":" + std::to_string(instance.playback.track_idx) +
						 ",\"runs\":" + runs_json(instance)});
			if (_probe) {
				probe_instance(*object, object->stage, i);
				probe_layers(*object, i, object->stage, object->tints);
			}
			const Camera& object_camera = e.space == asb::RuntimeEntity::Space::Ui ? ui_camera : camera;
			static const glm::mat4 screen_projection = glm::ortho(0.0f, 1920.0f, 1080.0f, 0.0f);
			const bool screen_chara = e.kind == "DrUiCreateChara";
			const glm::mat4& object_projection = screen_chara ? screen_projection : projection;
			// UI-space stage replacements are drawn with the fixed design camera, so
			// compensate with the field zoom here. World-space stages already get it
			// from Camera::lookAt(); applying it again double-zooms the background.
			const float camera_scale =
				object->replaces_stage && e.space == asb::RuntimeEntity::Space::Ui && camera.zoom() > 0.0f
					? camera.zoom()
					: 1.0f;
			float object_scale = (e.scale > 0.0f ? e.scale : 1.0f) * camera_scale;
			// TODO(ui): DrUiCreateChara positions and MBS bounds are in 1920x1080
			// screen space, but the engine's authored scale is larger than the
			// screen projection. 0.6 is calibrated against the opening montage;
			// recover the engine UI scale factor instead of keeping this constant.
			if (screen_chara)
				object_scale *= 0.6f;
			object->playback.asset->renderer.draw(
				instance,
				object_projection,
				object_camera,
				pos,
				object->tints.empty() ? nullptr : &object->tints,
				true,
				object_scale,
				alpha,
				flip,
				0.0f,
				e.layer_mask,
				e.scale_y > 0.0f ? e.scale_y * camera_scale : 0.0f);
		}
	}
}

void SceneRenderer::render_weather(const glm::mat4& projection, const Camera& camera) {
	if (std::getenv("VIEWER_NO_WEATHER"))
		return;
	const float amount = _weather_amount * _weather_request;
	if (amount <= 0.01f || _weather_regions.empty())
		return;
	const glm::mat4 mvp = projection * camera.lookAt();
	glm::vec2 ndc_min{1.0f, 1.0f};
	glm::vec2 ndc_max{-1.0f, -1.0f};
	bool any = false;
	for (const auto& region : _weather_regions) {
		if (!region.rain)
			continue;
		const auto& spec = region.spec;
		const auto& b = region.bounds;
		const float sy = spec.scale_y > 0.0f ? spec.scale_y : spec.scale;
		const glm::vec2 corners[4] = {
			{b.x, b.y},
			{b.z, b.y},
			{b.z, b.w},
			{b.x, b.w},
		};
		for (const auto& corner : corners) {
			const glm::vec4 clip = mvp * glm::vec4(spec.x + corner.x * spec.scale, spec.y + corner.y * sy, 0.0f, 1.0f);
			if (clip.w <= 0.0f)
				continue;
			const glm::vec2 ndc{clip.x / clip.w, clip.y / clip.w};
			ndc_min = glm::min(ndc_min, ndc);
			ndc_max = glm::max(ndc_max, ndc);
			any = true;
		}
	}
	if (!any)
		return;
	ndc_min = glm::clamp(ndc_min, glm::vec2(-1.0f), glm::vec2(1.0f));
	ndc_max = glm::clamp(ndc_max, glm::vec2(-1.0f), glm::vec2(1.0f));
	_weather.draw(ndc_min, ndc_max, amount, static_cast<float>(_frame) / 60.0f);
}

void SceneRenderer::clear() {
	_objects.clear();
	reset_stage();
}
