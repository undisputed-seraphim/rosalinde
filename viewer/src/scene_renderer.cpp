#include "scene_renderer.hpp"
#include "background_tracks.hpp"
#include "scene_util.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>

namespace {

bool track_usable(const SpriteData& data, int index) {
	return index >= 0 && static_cast<size_t>(index) < data.tracks.size() && !data.tracks[index].disabled &&
		   !data.tracks[index].runs.empty();
}

} // namespace

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

void SceneRenderer::set_stage(const std::string& mbs, const std::string& ftx) {
	if (mbs == _stage_mbs && ftx == _stage_ftx)
		return;
	_stage_mbs = mbs;
	_stage_ftx = ftx;
	_stage = Object{};
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
		if (std::getenv("SCENE_TRACE"))
			std::fprintf(
				stderr,
				"[bg] %s tracks=%zu selected=%zu\n",
				e.mbs.c_str(),
				object.asset->data.tracks.size(),
				selected.size());
		for (uint32_t i : selected) {
			if (!track_usable(object.asset->data, static_cast<int>(i)))
				continue;
			object.instances.emplace_back(&object.asset->data, i, 0xFFFFFFFFu);
			object.instances.back().hold_finished = object.background || e.name != "ヴァニラロゴ";
			object.instances.back().play(i);
			if (std::getenv("SCENE_TRACE"))
				std::fprintf(
					stderr,
					"  [bg] track=%u name=%s runs=%zu\n",
					i,
					object.asset->data.tracks[i].name.c_str(),
					object.asset->data.tracks[i].runs.size());
		}
	} else {
		const int track = choose_track(*object.asset, e);
		if (track < 0)
			return;
		object.track_index = track;
		object.instances.emplace_back(&object.asset->data, static_cast<uint32_t>(track), 0xFFFFFFFFu);
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

		auto it = _objects.find(e.name);
		if (it != _objects.end()) {
			const bool same_asset = it->second.state.mbs == e.mbs && it->second.state.ftx == e.ftx;
			if (same_asset) {
				apply(it->second, e);
				continue;
			}
			_objects.erase(it);
		}

		auto asset = _library.get(e.mbs, e.ftx);
		if (!asset)
			continue;
		Object object;
		rebuild(object, e, std::move(asset));
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
			e.order = -100000;
			e.alpha = 255;
			rebuild(_stage, e, std::move(asset));
		}
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

void SceneRenderer::render(const glm::mat4& projection, const Camera& camera) {
	enable_blend(glm::vec4(1.0f));
	enable_depth(GL_ALWAYS);
	glClear(GL_DEPTH_BUFFER_BIT);

	std::vector<Object*> order;
	order.reserve(_objects.size());
	for (auto& [_, object] : _objects)
		if (object.asset && !object.instances.empty() && object.state.visible)
			order.push_back(&object);
	if (_stage.asset && !_stage.instances.empty() && _stage.state.visible)
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
			objects += (objects.empty() ? "" : ",");
			objects += "{\"name\":\"" + asb::trace::quote(e.name) + "\",\"order\":" + asb::trace::integer(e.order) +
					   ",\"instances\":" + asb::trace::integer(static_cast<long>(object->instances.size())) +
					   ",\"track\":" + asb::trace::integer(object->track_index) +
					   ",\"pos\":" + asb::trace::vec2(e.pos) + ",\"scale\":" + asb::trace::number(e.scale) +
					   ",\"alpha\":" + asb::trace::integer(e.alpha) +
					   ",\"background\":" + asb::trace::boolean(object->background) +
					   ",\"bounds\":" + asb::trace::vec4(&bounds.x) + "}";
		}
		_trace({_frame, "draw", "list", "\"objects\":[" + objects + "]"});
	}
	const char* hide = std::getenv("VIEWER_HIDE");
	for (Object* object : order) {
		const auto& e = object->state;
		if (hide && !e.name.empty() && e.name.find(hide) != std::string::npos)
			continue;
		const glm::vec2 offset = parent_position(*object);
		const glm::vec2 pos{e.pos[0] + offset.x, e.pos[1] + offset.y};
		const float alpha = std::clamp(e.alpha / 255.0f, 0.0f, 1.0f);
		if (alpha <= 0.0f)
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

		if (std::getenv("VIEWER_TRACE_SPRITES")) {
			std::fprintf(
				stderr,
				"[draw] %s pos=(%.1f,%.1f) instances=%zu\n",
				e.name.c_str(),
				pos.x,
				pos.y,
				object->instances.size());
			for (size_t i = 0; i < object->instances.size(); ++i) {
				const auto& in = object->instances[i];
				const auto b = in.render_bounds();
				std::fprintf(
					stderr,
					"  [draw] %zu track=%u sa=%u bounds=(%.1f,%.1f,%.1f,%.1f)\n",
					i,
					in.track_idx,
					in.sa_count(),
					b.x,
					b.y,
					b.z,
					b.w);
			}
		}
		for (auto& instance : object->instances) {
			object->asset->renderer.draw(
				instance,
				projection,
				camera,
				pos,
				tints.empty() ? nullptr : &tints,
				object->background,
				e.scale > 0.0f ? e.scale : 1.0f,
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
}
