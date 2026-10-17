#include "effect_manager.hpp"

#include <algorithm>
#include <cmath>

namespace {

float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

float ease(float t) {
	t = clamp01(t);
	return t * t * (3.0f - 2.0f * t);
}

} // namespace

void EffectManager::reset() {
	_effects.clear();
	_assets.clear();
	_next = 0;
}

std::shared_ptr<EffectManager::Asset> EffectManager::asset_for(const std::string& mbs, const std::string& ftx) {
	const std::string key = mbs + "|" + ftx;
	if (auto it = _assets.find(key); it != _assets.end())
		return it->second;

	auto mbs_path = _loader.resolve(mbs);
	if (!mbs_path)
		return {};
	Job job;
	job.mbs = *mbs_path;
	auto ftx_path = ftx.empty() ? _loader.resolve_ftx(mbs, mbs) : _loader.resolve_ftx(ftx, mbs);
	if (ftx_path)
		job.ftx = *ftx_path;

	auto layer = _loader.load_layer(job, 0, mbs, {});
	if (!layer)
		return {};
	auto asset = std::make_shared<Asset>();
	asset->data = std::move(layer->data);
	asset->renderer = std::move(layer->renderer);
	_assets[key] = asset;
	return asset;
}

long EffectManager::track_frames(const SpriteData& data, int track) {
	if (track < 0 || static_cast<size_t>(track) >= data.tracks.size())
		return 0;
	long n = 0;
	for (const auto& run : data.tracks[static_cast<size_t>(track)].runs) {
		for (uint32_t i = 0; i < run.s8_count; ++i) {
			const size_t idx = static_cast<size_t>(run.s8_start) + i;
			if (idx < data.v77.s8.size())
				n += data.v77.s8[idx].frames;
		}
	}
	return n;
}

void EffectManager::apply(const asb::RuntimeEffect& e, long frame, const EntityPos& entity_pos) {
	(void)frame;
	(void)entity_pos;
	if (e.kind == "delete") {
		_effects.erase(e.name);
		return;
	}
	if (e.kind == "create") {
		auto asset = asset_for(e.mbs, e.ftx);
		if (!asset || asset->data.tracks.empty())
			return;
		const int track = std::clamp(e.anime, 0, static_cast<int>(asset->data.tracks.size()) - 1);
		Effect ef;
		ef.name = e.name.empty() ? "effect#" + std::to_string(_next++) : e.name;
		ef.asset = std::move(asset);
		ef.instance.data = &ef.asset->data;
		ef.instance.track_idx = static_cast<uint32_t>(track);
		ef.instance.play(static_cast<uint32_t>(track));
		ef.base_pos = {e.pos[0], e.pos[1]};
		ef.pos = ef.base_pos;
		ef.scale = e.f0 > 0.0f ? e.f0 : 1.0f;
		ef.start = e.frame;
		ef.fade_in = std::max(0, e.i0);
		ef.auto_delete = e.flag;
		ef.end = e.frame + track_frames(ef.asset->data, track);
		_effects[ef.name] = std::move(ef);
		return;
	}

	auto it = _effects.find(e.name);
	if (it == _effects.end())
		return;
	Effect& ef = it->second;

	if (e.kind == "dir") {
		ef.dir_x = e.i0 ? 1.0f : -1.0f;
	} else if (e.kind == "attach") {
		ef.attached = e.attached;
	} else if (e.kind == "sprite_attach") {
		ef.attached = e.attached;
		ef.offset = {e.pos[0], e.pos[1]};
	} else if (e.kind == "move") {
		ef.moving = true;
		ef.move_from = ef.pos;
		ef.move_to = {e.pos[0], e.pos[1]};
		ef.move_f0 = e.frame;
		ef.move_f1 = e.frame + std::max(1, e.i0);
	} else if (e.kind == "rotate") {
		ef.rotating = true;
		ef.rot_from = ef.rot;
		ef.rot_to = ef.rot + e.f0 * 3.14159265f / 180.0f;
		ef.rot_f0 = e.frame;
		ef.rot_f1 = e.frame + std::max(1, e.i0);
	} else if (e.kind == "scale") {
		ef.scaling = true;
		ef.scale_from = ef.scale;
		ef.scale_to = e.f0;
		ef.scale_f0 = e.frame;
		ef.scale_f1 = e.frame + std::max(1, e.i0);
	}
}

void EffectManager::update(long frame, const std::vector<asb::RuntimeEffect>& events, const EntityPos& entity_pos) {
	while (_next < events.size() && events[_next].frame <= frame) {
		apply(events[_next], frame, entity_pos);
		++_next;
	}

	for (auto it = _effects.begin(); it != _effects.end();) {
		Effect& ef = it->second;
		if (ef.attached.empty()) {
			ef.pos = ef.base_pos;
		} else {
			ef.pos = entity_pos(ef.attached) + ef.offset;
		}

		if (ef.moving) {
			const float t = ease(float(frame - ef.move_f0) / float(std::max<long>(1, ef.move_f1 - ef.move_f0)));
			ef.pos = glm::mix(ef.move_from, ef.move_to, t);
		}
		if (ef.rotating) {
			const float t = ease(float(frame - ef.rot_f0) / float(std::max<long>(1, ef.rot_f1 - ef.rot_f0)));
			ef.rot = ef.rot_from + (ef.rot_to - ef.rot_from) * t;
		}
		if (ef.scaling) {
			const float t = ease(float(frame - ef.scale_f0) / float(std::max<long>(1, ef.scale_f1 - ef.scale_f0)));
			ef.scale = ef.scale_from + (ef.scale_to - ef.scale_from) * t;
		}
		ef.alpha = ef.fade_in > 0 ? clamp01(float(frame - ef.start) / float(ef.fade_in)) : 1.0f;

		if (ef.auto_delete && ef.end > ef.start && frame > ef.end)
			it = _effects.erase(it);
		else
			++it;
	}
}

void EffectManager::tick(float dt) {
	for (auto& [name, ef] : _effects) {
		if (ef.alpha > 0.0f)
			ef.instance.update(dt);
	}
}

void EffectManager::draw(const glm::mat4& projection, const Camera& cam, const glm::vec2& shake) {
	for (auto& [name, ef] : _effects) {
		if (!ef.asset || ef.alpha <= 0.0f)
			continue;
		ef.asset->renderer.draw(
			ef.instance,
			projection,
			cam,
			ef.pos + shake,
			nullptr,
			false,
			ef.scale,
			ef.alpha,
			ef.dir_x < 0.0f,
			ef.rot);
	}
}
