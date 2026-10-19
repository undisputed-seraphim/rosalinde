#include "scene_renderer.hpp"
#include "background_tracks.hpp"
#include "probe.hpp"
#include "scene_util.hpp"

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

bool track_usable(const SpriteData& data, int index) {
	return index >= 0 && static_cast<size_t>(index) < data.tracks.size() && !data.tracks[index].disabled &&
		   !data.tracks[index].runs.empty();
}

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
		if (instance.data && instance.track_idx < instance.data->tracks.size() &&
			i < instance.data->tracks[instance.track_idx].runs.size())
			track_slot = instance.data->tracks[instance.track_idx].runs[i].sa_track_slot;
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
	view.alpha = static_cast<int>(std::lround(view.fade_from * (1.0f - t)));
	return view;
}

} // namespace

bool SceneRenderer::stage_replaced() const {
	for (const auto& [_, object] : _objects) {
		if (object.background && object.state.started && object.state.visible && object.state.prerender_force)
			return true;
	}
	return false;
}

int SceneRenderer::choose_track(const SpriteAsset& asset, const asb::RuntimeEntity& e) {
	if (track_usable(asset.data, e.track_index))
		return e.track_index;
	if (!e.track.empty()) {
		if (const auto* t = asset.data.find_track(e.track))
			return static_cast<int>(t - asset.data.tracks.data());
	}
	for (size_t i = 0; i < asset.data.tracks.size(); ++i)
		if (track_usable(asset.data, static_cast<int>(i)))
			return static_cast<int>(i);
	return -1;
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

void SceneRenderer::set_stage(const std::string& mbs, const std::string& ftx, std::vector<StageLayer> layers) {
	if (mbs == _stage_mbs && ftx == _stage_ftx && layers.empty() && _stage_layers.empty())
		return;
	_stage_mbs = mbs;
	_stage_ftx = ftx;
	_stage = Object{};
	_stage.state.visible = true;
	_stage.state.space =
		mbs.rfind("FieldBG/", 0) == 0 || !layers.empty() ? asb::RuntimeEntity::Space::Ui : asb::RuntimeEntity::Space::World;
	_stage_layers.clear();
	_stage_layers.reserve(layers.size());
	for (auto& layer : layers)
		_stage_layers.push_back({std::move(layer), nullptr, false});
}

void SceneRenderer::rebuild(Object& object, const asb::RuntimeEntity& e, SpriteAssetPtr asset) {
	object.state = e;
	object.asset = std::move(asset);
	object.instances.clear();
	object.track_index = -1;
	object.background = object.asset && object.asset->is_background;

	if (!object.asset)
		return;

	if (object.background) {
		const auto selected = select_background_tracks(object.asset->data.v77);
		spdlog::debug("bg: {} tracks={} selected={}", e.mbs, object.asset->data.tracks.size(), selected.size());
		for (uint32_t i : selected) {
			if (!track_usable(object.asset->data, static_cast<int>(i)))
				continue;
			object.instances.emplace_back(&object.asset->data, i, 0xFFFFFFFFu);
			object.instances.back().hold_finished = true;
			object.instances.back().flip_v = flip_v_for(e);
			object.instances.back().play(i);
			spdlog::debug(
				"bg:   track={} name={} runs={}",
				i,
				object.asset->data.tracks[i].name,
				object.asset->data.tracks[i].runs.size());
		}
	} else {
		const int track = choose_track(*object.asset, e);
		if (track < 0)
			return;
		object.track_index = track;
		object.instances.emplace_back(&object.asset->data, static_cast<uint32_t>(track), 0xFFFFFFFFu);
		object.instances.back().flip_v = flip_v_for(e);
		object.instances.back().play(static_cast<uint32_t>(track));
	}
	apply(object, e);
	if (_trace)
		_trace(
			{_frame,
			 "sprite",
			 "rebuild",
			 "\"name\":\"" + asb::trace::quote(e.name) + "\",\"mbs\":\"" + asb::trace::quote(e.mbs) + "\",\"ftx\":\"" +
				 asb::trace::quote(e.ftx) + "\",\"background\":" + asb::trace::boolean(object.background) +
				 ",\"instances\":" + asb::trace::integer(static_cast<long>(object.instances.size()))});
}

void SceneRenderer::apply(Object& object, const asb::RuntimeEntity& e) {
	object.state = e;
	if (object.instances.empty())
		return;
	if (!object.background) {
		const int track = choose_track(*object.asset, e);
		if (track >= 0 && track != object.track_index) {
			object.track_index = track;
			object.instances[0].play(static_cast<uint32_t>(track));
			if (_trace)
				_trace(
					{_frame,
					 "sprite",
					 "track",
					 "\"name\":\"" + asb::trace::quote(e.name) + "\",\"track\":" + asb::trace::integer(track)});
		}
	}
	for (auto& instance : object.instances)
		instance.paused = e.paused || e.anim_paused;
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
		Object object;
		rebuild(object, view, std::move(asset));
		_objects.emplace(e.name, std::move(object));
	}

	for (auto it = _objects.begin(); it != _objects.end();) {
		if (!seen.contains(it->first))
			it = _objects.erase(it);
		else
			++it;
	}

	if (!_stage_mbs.empty() && !_stage_ftx.empty() && (!_stage.asset || _stage.state.mbs != _stage_mbs)) {
		auto asset = _library.get(_stage_mbs, _stage_ftx);
		if (asset) {
			asb::RuntimeEntity e;
			e.name = "__stage__";
			e.kind = "StageBG";
			e.mbs = _stage_mbs;
			e.ftx = _stage_ftx;
			e.space = _stage_mbs.rfind("FieldBG/", 0) == 0 ? asb::RuntimeEntity::Space::Ui
															 : asb::RuntimeEntity::Space::World;
			e.order = -100000;
			e.alpha = 255;
			rebuild(_stage, e, std::move(asset));
		}
	}

	for (auto& layer : _stage_layers) {
		if (layer.attempted)
			continue;
		layer.attempted = true;
		layer.asset = MpdAsset::load(_library.loader(), layer.spec.mpd, layer.spec.ftx);
		if (!layer.asset)
			spdlog::warn("stage: failed to load MPD {} ({})", layer.spec.mpd, layer.spec.ftx);
	}
}

void SceneRenderer::advance(uint32_t frames) {
	if (frames == 0)
		return;
	for (auto& [_, object] : _objects)
		for (auto& instance : object.instances)
			instance.advance_frames(frames);
	for (auto& instance : _stage.instances)
		instance.advance_frames(frames);
}

glm::vec2 SceneRenderer::parent_position(const Object& object) const {
	if (object.state.parent.empty())
		return {0.0f, 0.0f};
	auto it = _objects.find(object.state.parent);
	if (it == _objects.end())
		return {0.0f, 0.0f};
	return {it->second.state.pos[0], it->second.state.pos[1]};
}

std::string SceneRenderer::object_json(const Object& object, bool stage, std::string_view reason) const {
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
		",\"background\":" + asb::trace::boolean(object.background) +
		",\"asset_loaded\":" + asb::trace::boolean(object.asset != nullptr);

	std::string track_name;
	if (object.asset && object.track_index >= 0 &&
		static_cast<size_t>(object.track_index) < object.asset->data.tracks.size())
		track_name = object.asset->data.tracks[object.track_index].name;
	out += ",\"track_index\":" + std::to_string(object.track_index) + ",\"track_name\":" + quote(track_name);

	out += ",\"instances\":[";
	for (size_t i = 0; i < object.instances.size(); ++i) {
		const auto& instance = object.instances[i];
		if (i)
			out += ',';
		std::string instance_track;
		if (instance.data && instance.track_idx < instance.data->tracks.size())
			instance_track = instance.data->tracks[instance.track_idx].name;
		out += "{\"index\":" + std::to_string(i) + ",\"track\":" + std::to_string(instance.track_idx) +
			   ",\"track_name\":" + quote(instance_track) +
			   ",\"variant_flags\":" + std::to_string(instance.variant_flags) +
			   ",\"frame_counter\":" + std::to_string(instance.frame_counter()) +
			   ",\"hold_finished\":" + asb::trace::boolean(instance.hold_finished) +
			   ",\"bounds\":" + vec4_json(instance.render_bounds()) + ",\"runs\":" + runs_json(instance) + "}";
	}
	out += "]";

	glm::vec4 bounds{
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity()};
	for (const auto& instance : object.instances) {
		const auto b = instance.render_bounds();
		bounds.x = std::min(bounds.x, b.x);
		bounds.y = std::min(bounds.y, b.y);
		bounds.z = std::max(bounds.z, b.z);
		bounds.w = std::max(bounds.w, b.w);
	}
	out += ",\"bounds\":" + vec4_json(bounds) + ",\"reason\":" + quote(reason) + "}";
	return out;
}

void SceneRenderer::probe_object(const Object& object, bool stage, std::string_view reason) {
	if (!_probe)
		return;
	_probe->draw() << "{\"frame\":" << _frame
				   << ",\"kind\":\"draw\",\"event\":\"object\",\"object\":" << object_json(object, stage, reason)
				   << "}\n";
}

void SceneRenderer::probe_instance(const Object& object, bool stage, size_t index) {
	if (!_probe || index >= object.instances.size())
		return;
	const auto& instance = object.instances[index];
	std::string track_name;
	if (instance.data && instance.track_idx < instance.data->tracks.size())
		track_name = instance.data->tracks[instance.track_idx].name;
	_probe->draw() << "{\"frame\":" << _frame
				   << ",\"kind\":\"draw\",\"event\":\"instance\",\"object\":" << quote(object.state.name)
				   << ",\"stage\":" << asb::trace::boolean(stage) << ",\"instance\":" << index
				   << ",\"track\":" << instance.track_idx << ",\"track_name\":" << quote(track_name)
				   << ",\"variant_flags\":" << instance.variant_flags
				   << ",\"frame_counter\":" << instance.frame_counter()
				   << ",\"bounds\":" << vec4_json(instance.render_bounds()) << ",\"runs\":" << runs_json(instance)
				   << "}\n";
}

void SceneRenderer::probe_layers(
	const Object& object,
	size_t index,
	bool stage,
	const std::map<uint32_t, glm::vec4>& tints) {
	if (!_probe || index >= object.instances.size())
		return;
	const auto& instance = object.instances[index];
	if (!instance.data || instance.track_idx >= instance.data->tracks.size())
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

				_probe->draw() << "{\"frame\":" << _frame
							   << ",\"kind\":\"draw\",\"event\":\"layer\",\"object\":" << quote(object.state.name)
							   << ",\"stage\":" << asb::trace::boolean(stage) << ",\"instance\":" << index
							   << ",\"run\":" << layer.run << ",\"layer\":" << layer.layer
							   << ",\"track\":" << instance.track_idx << ",\"track_id\":" << layer.sa_track_slot
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
							   << ",\"blend\":" << static_cast<unsigned>(layer.blend)
							   << ",\"attributes\":" << layer.attributes
							   << ",\"notex\":" << asb::trace::boolean(layer.notex)
							   << ",\"flip_v\":" << asb::trace::boolean(layer.flip_v)
							   << ",\"drawn\":" << asb::trace::boolean(layer.drawn) << ",\"skip\":" << quote(layer.skip)
							   << ",\"depth\":" << number(layer.depth)
							   << ",\"transition_t\":" << number(layer.transition_t)
							   << ",\"s6_bounds\":" << vec4_json(layer.s6_bounds)
							   << ",\"s7_move\":" << vec3_json(layer.s7_move)
							   << ",\"s7_rotate\":" << vec3_json(layer.s7_rotate)
							   << ",\"s7_scale\":" << vec2_json(layer.s7_scale) << ",\"s7_fog\":" << layer.s7_fog
							   << ",\"s1_center\":" << vec2_json(layer.s1_center)
							   << ",\"s1_topleft\":" << vec2_json(layer.s1_topleft) << ",\"uv\":" << uv
							   << ",\"uv_norm\":" << uv_norm << ",\"xy\":" << xy
							   << ",\"raw_color\":" << color_json(layer.raw_color)
							   << ",\"color\":" << color_json(layer.color) << "}\n";
			});
	}
}

std::string SceneRenderer::state_json(const asb::SceneState& scene) const {
	std::vector<const Object*> objects;
	objects.reserve(_objects.size() + 1);
	for (const auto& [_, object] : _objects)
		objects.push_back(&object);
	if (_stage.asset)
		objects.push_back(&_stage);
	std::sort(objects.begin(), objects.end(), [](const Object* a, const Object* b) {
		if (a->state.order != b->state.order)
			return a->state.order < b->state.order;
		return a->state.name < b->state.name;
	});

	std::string out = "{\"objects\":[";
	bool first = true;
	for (const Object* object : objects) {
		if (!first)
			out += ',';
		first = false;
		const bool stage = object == &_stage;
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
	std::vector<const Object*> objects;
	objects.reserve(_objects.size() + 1);
	for (const auto& [_, object] : _objects)
		objects.push_back(&object);
	if (_stage.asset && !stage_replaced())
		objects.push_back(&_stage);
	std::sort(objects.begin(), objects.end(), [](const Object* a, const Object* b) {
		if (a->state.order != b->state.order)
			return a->state.order < b->state.order;
		return a->state.name < b->state.name;
	});

	std::string out = "{\"scene_frame\":" + std::to_string(scene.frame) + ",\"objects\":[";
	bool first = true;
	for (const Object* object : objects) {
		if (!first)
			out += ',';
		first = false;
		const auto& e = object->state;
		out += "{\"name\":" + quote(e.name) + ",\"kind\":" + quote(e.kind) + ",\"mbs\":" + quote(e.mbs) +
			   ",\"order\":" + std::to_string(e.order) + ",\"alpha\":" + std::to_string(e.alpha) +
			   ",\"visible\":" + asb::trace::boolean(e.visible) +
			   ",\"started\":" + asb::trace::boolean(e.started) +
			   ",\"background\":" + asb::trace::boolean(object->background) + ",\"track\":" +
			   std::to_string(object->track_index) + ",\"instances\":[";
		for (size_t i = 0; i < object->instances.size(); ++i) {
			if (i)
				out += ',';
			const auto& instance = object->instances[i];
			std::string track_name;
			if (instance.data && instance.track_idx < instance.data->tracks.size())
				track_name = instance.data->tracks[instance.track_idx].name;
			out += "{\"index\":" + std::to_string(i) + ",\"track\":" + std::to_string(instance.track_idx) +
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
	std::vector<const Object*> objects;
	objects.reserve(_objects.size() + 1);
	for (const auto& [_, object] : _objects)
		objects.push_back(&object);
	if (_stage.asset)
		objects.push_back(&_stage);
	std::sort(objects.begin(), objects.end(), [](const Object* a, const Object* b) {
		if (a->state.order != b->state.order)
			return a->state.order < b->state.order;
		return a->state.name < b->state.name;
	});
	for (const Object* object : objects) {
		const bool stage = object == &_stage;
		std::string track_name;
		if (object->asset && object->track_index >= 0 &&
			static_cast<size_t>(object->track_index) < object->asset->data.tracks.size())
			track_name = object->asset->data.tracks[object->track_index].name;
		out << object->state.name << " track=" << object->track_index << " name=" << track_name
			<< " instances=" << object->instances.size() << " reason=" << filter_reason(object->state, stage) << '\n';
	}
	return out.str();
}

void SceneRenderer::render(const glm::mat4& projection, const Camera& camera) {
	enable_blend(glm::vec4(1.0f));
	enable_depth(GL_ALWAYS);
	glClear(GL_DEPTH_BUFFER_BIT);

	static const Camera ui_camera;
	if (_stage.state.visible && !stage_replaced()) {
		const Camera& stage_camera = _stage.state.space == asb::RuntimeEntity::Space::Ui ? ui_camera : camera;
		for (auto& layer : _stage_layers)
			if (layer.asset)
				layer.asset->draw(projection, stage_camera);
	}

	std::vector<Object*> order;
	order.reserve(_objects.size());
	for (auto& [_, object] : _objects)
		if (object.asset && !object.instances.empty() && object.state.visible)
			order.push_back(&object);
	if (_stage.asset && !_stage.instances.empty() && _stage.state.visible && !stage_replaced())
		order.push_back(&_stage);

	std::stable_sort(order.begin(), order.end(), [](const Object* a, const Object* b) {
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

	if (_trace) {
		std::string objects;
		for (Object* object : order) {
			const auto& e = object->state;
			glm::vec4 bounds{
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity(),
				-std::numeric_limits<float>::infinity(),
				-std::numeric_limits<float>::infinity()};
			for (const auto& instance : object->instances) {
				const auto b = instance.render_bounds();
				bounds.x = std::min(bounds.x, b.x);
				bounds.y = std::min(bounds.y, b.y);
				bounds.z = std::max(bounds.z, b.z);
				bounds.w = std::max(bounds.w, b.w);
			}
			std::string track_name;
			if (object->asset && object->track_index >= 0 &&
				static_cast<size_t>(object->track_index) < object->asset->data.tracks.size())
				track_name = object->asset->data.tracks[object->track_index].name;
			objects += (objects.empty() ? "" : ",");
			objects += "{\"name\":\"" + asb::trace::quote(e.name) + "\",\"order\":" + asb::trace::integer(e.order) +
					   ",\"instances\":" + asb::trace::integer(static_cast<long>(object->instances.size())) +
					   ",\"track\":" + asb::trace::integer(object->track_index) + ",\"track_name\":\"" +
					   asb::trace::quote(track_name) + "\"" +
					   ",\"prerender_force\":" + asb::trace::boolean(e.prerender_force) +
					   ",\"window_id_offset\":" + asb::trace::integer(e.window_id_offset) +
					   ",\"dlprio\":" + asb::trace::integer(e.dlprio) +
					   ",\"sort_id_offset\":" + asb::trace::integer(e.sort_id_offset) + ",\"space\":\"" +
					   (e.space == asb::RuntimeEntity::Space::Ui ? "ui" : "world") + "\"" +
					   ",\"pos\":" + asb::trace::vec2(e.pos) + ",\"scale\":" + asb::trace::number(e.scale) +
					   ",\"alpha\":" + asb::trace::integer(e.alpha) +
					   ",\"background\":" + asb::trace::boolean(object->background) + ",\"reason\":\"" +
					   filter_reason(e, object == &_stage) + "\"" + ",\"bounds\":" + asb::trace::vec4(&bounds.x) + "}";
		}
		_trace({_frame, "draw", "list", "\"objects\":[" + objects + "]"});
	}
	for (Object* object : order) {
		const auto& e = object->state;
		const glm::vec2 offset = parent_position(*object);
		const glm::vec2 pos{e.pos[0] + offset.x, e.pos[1] + offset.y};
		const float alpha = std::clamp(e.alpha / 255.0f, 0.0f, 1.0f);
		std::string reason = filter_reason(e, object == &_stage);
		if (reason.empty() && alpha <= 0.0f)
			reason = "alpha";
		if (_probe)
			probe_object(*object, object == &_stage, reason);
		if (!reason.empty())
			continue;
		const bool flip = (e.dir_x < 0.0f) != e.reverse_x;

		std::map<uint32_t, glm::vec4> tints;
		const int packed = e.color[0] >= 0 ? e.color[0] : e.color[2];
		if (packed >= 0) {
			const glm::vec4 tint{
				((packed >> 16) & 0xFF) / 255.0f, ((packed >> 8) & 0xFF) / 255.0f, (packed & 0xFF) / 255.0f, 1.0f};
			tints[0] = tint;
			for (const auto& s4 : object->asset->data.v77.s4)
				tints[s4.attributes] = tint;
		}

		spdlog::debug("draw: {} pos=({:.1f},{:.1f}) instances={}", e.name, pos.x, pos.y, object->instances.size());
		for (size_t i = 0; i < object->instances.size(); ++i) {
			const auto& in = object->instances[i];
			const auto b = in.render_bounds();
			spdlog::debug(
				"draw:   {} track={} sa={} bounds=({:.1f},{:.1f},{:.1f},{:.1f})",
				i,
				in.track_idx,
				in.sa_count(),
				b.x,
				b.y,
				b.z,
				b.w);
			const auto states = in.run_states();
			for (size_t k = 0; k < states.size(); ++k)
				spdlog::debug(
					"draw:     run={} offset={} flags=0x{:X} frames={} done={} hidden={}",
					k,
					states[k].offset,
					states[k].flags,
					states[k].frames,
					states[k].done,
					states[k].hidden);
		}
		for (size_t i = 0; i < object->instances.size(); ++i) {
			auto& instance = object->instances[i];
			if (_trace)
				_trace({_frame,
						"draw",
						"runs",
						"\"object\":\"" + asb::trace::quote(e.name) + "\",\"instance\":" + std::to_string(i) +
							",\"track\":" + std::to_string(instance.track_idx) + ",\"runs\":" +
							runs_json(instance)});
			if (_probe) {
				probe_instance(*object, object == &_stage, i);
				probe_layers(*object, i, object == &_stage, tints);
			}
			const Camera& object_camera = e.space == asb::RuntimeEntity::Space::Ui ? ui_camera : camera;
			const float camera_scale =
				object->background && e.prerender_force && camera.zoom() > 0.0f ? camera.zoom() : 1.0f;
			const float object_scale = (e.scale > 0.0f ? e.scale : 1.0f) * camera_scale;
			object->asset->renderer.draw(
				instance,
				projection,
				object_camera,
				pos,
				tints.empty() ? nullptr : &tints,
				true,
				object_scale,
				alpha,
				flip,
				0.0f,
				e.layer_mask);
		}
	}
}

void SceneRenderer::clear() {
	_objects.clear();
	_stage = Object{};
	_stage_mbs.clear();
	_stage_ftx.clear();
	_stage_layers.clear();
}
