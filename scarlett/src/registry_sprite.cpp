#include "native_args.hpp"
#include "registry.hpp"

#include <angelscript.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace asb {

namespace {

bool plausible_scale(float value) { return std::isfinite(value) && value > 1e-4f && value < 1e4f; }

// DrSetLayerMask / DrSetVariationType take a hexadecimal bitmask string.
unsigned parse_hex(const std::string& text) {
	unsigned value = 0;
	size_t i = (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) ? 2 : 0;
	for (; i < text.size(); ++i) {
		const char c = text[i];
		int digit = (c >= '0' && c <= '9')	 ? c - '0'
					: (c >= 'a' && c <= 'f') ? c - 'a' + 10
					: (c >= 'A' && c <= 'F') ? c - 'A' + 10
											 : -1;
		if (digit < 0)
			break;
		value = (value << 4) | static_cast<unsigned>(digit);
	}
	return value;
}

} // namespace

DispatchResult Registry::dispatch_sprite(std::string_view name, NativeArgs& args) {
	asIScriptGeneric* gen = args.generic();
	auto S = [&](int i) { return args.str(i); };
	auto F = [&](int i) { return args.f32(i); };
	auto I = [&](int i) { return args.i32(i); };
	auto V = [&](int i, float* out, int count) {
		if (count == 2)
			args.vec2(i, out);
		else
			args.vec3(i, out);
	};
	auto ent = [&](int i) { return entity_arg(args, i); };

	// Scene queries: answer from the recorded scene so branches and computed
	// args see real values instead of the stub's zero.
	if (name == "IsObjectExist" || name == "IsExistSprite") {
		auto it = _by_name.find(S(0));
		const bool exists = it != _by_name.end() && _timeline.entities[it->second].visible;
		gen->SetReturnByte(exists ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "GetObjectNameByCharaDefName") {
		// Reverse lookup over the live chara objects: find the one whose chara def
		// name matches and return its object name; "NONE" when absent (as the
		// engine does).
		const std::string def_name = S(0);
		std::string result = "NONE";
		for (const auto& entity : _timeline.entities) {
			if (!entity.visible)
				continue;
			const bool is_chara =
				entity.kind == "DrCreateChara" || entity.kind == "DrCreateNpc" || entity.kind == "DrUiCreateChara";
			bool match = entity.name == def_name;
			for (const auto& alias : entity.aliases)
				match = match || alias == def_name;
			if (is_chara && match) {
				result = entity.aliases.empty() ? entity.name : entity.aliases.front();
				break;
			}
		}
		// Returning true skips the stub's default return construction, so the
		// value-type return slot must be constructed here (not assigned to).
		if (auto* out = static_cast<std::string*>(gen->GetAddressOfReturnLocation()))
			new (out) std::string(std::move(result));
		return DispatchResult::Handled;
	}
	if (name == "GetPosV") {
		float* result = static_cast<float*>(gen->GetAddressOfReturnLocation());
		if (result) {
			result[0] = 0.0f;
			result[1] = 0.0f;
			if (RuntimeEntity* entity = ent(0)) {
				result[0] = entity->pos[0];
				result[1] = entity->pos[1];
			}
		}
		return DispatchResult::Handled;
	}
	if (name == "DrGetPosV" || name == "DrGetCollisionPosV") {
		float* result = static_cast<float*>(gen->GetAddressOfReturnLocation());
		if (result) {
			result[0] = result[1] = result[2] = 0.0f;
			if (RuntimeEntity* entity = ent(0))
				for (int k = 0; k < 3; ++k)
					result[k] = entity->pos[k];
		}
		return DispatchResult::Handled;
	}
	if (name == "DrGetDirX") {
		int direction = 1;
		if (RuntimeEntity* entity = ent(0))
			direction = entity->dir_x < 0 ? -1 : 1;
		gen->SetReturnDWord(static_cast<asDWORD>(direction));
		return DispatchResult::Handled;
	}

	// CreateSpriteMone(name, mbs, ftx, track, FnVector2 pos, layer, scale, bool,
	// int, int, int): the first three string args are the id, the .mbs and the
	// .ftx (pushed in reverse, so S(0) is the name the mutators key on).
	if (name == "CreateSpriteMone" || name == "CreateSpriteEffect" || name == "CreateSpriteMoneWorld") {
		float position[3] = {0.0f, 0.0f, 0.0f};
		V(4, position, 2);
		create_sprite(std::string(name), S(0), S(1), S(2), position, I(5), F(6));
		return DispatchResult::Handled;
	}
	if (name == "CreateSpriteMoneStageStart") {
		float position[3] = {0.0f, 0.0f, 0.0f};
		V(1, position, 2);
		create_sprite(std::string(name), S(0), "", "", position, I(2), F(3));
		return DispatchResult::Handled;
	}
	if (name == "CreateSpriteCoA") {
		float position[3] = {0.0f, 0.0f, 0.0f};
		V(2, position, 2);
		create_sprite(std::string(name), S(0), "", "", position, I(3), F(4));
		return DispatchResult::Handled;
	}

	// Stage/weather/flag configuration. No id->effect mapping is known, so the
	// visual is left to the engine; record the ids for offline inspection.
	if (name == "DrCreateStage") {
		StageConfig stage;
		stage.frame = frame;
		stage.stage_type = I(0);
		stage.weather_type = I(1);
		stage.coa_preset = I(2);
		stage.flag_color = I(3);
		_timeline.stage = stage;
		_timeline.stages.push_back(std::move(stage));
		trace_event(
			"stage",
			"set",
			"\"type\":" + trace::integer(stage.stage_type) + ",\"weather\":" + trace::integer(stage.weather_type) +
				",\"coa\":" + trace::integer(stage.coa_preset) + ",\"flag\":" + trace::integer(stage.flag_color));
		return DispatchResult::Handled;
	}

	if (name == "AddOpeningResource") {
		const std::string path = S(0);
		auto& resources = _timeline.opening_resources;
		if (!path.empty() && std::find(resources.begin(), resources.end(), path) == resources.end()) {
			resources.push_back(path);
			trace_event("state", "opening_resource", "\"path\":\"" + trace::quote(path) + "\"");
		}
		return DispatchResult::Handled;
	}

	if (name == "DrUiCreateChara") {
		create_chara(std::string(name), {S(0), S(1), S(2), S(3)});
		return DispatchResult::Handled;
	}
	if (name == "DrCreateChara" || name == "DrCreateNpc") {
		create_chara(std::string(name), {S(0), S(1)});
		return DispatchResult::Handled;
	}

	// Anchor one sprite to another: the child's position becomes relative to
	// the target's (both are usually at the origin, but moving parents matter).
	if (name == "SetSpriteToAnchor") {
		if (RuntimeEntity* entity = ent(0)) {
			entity->parent = S(1);
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "SetSpriteScale" || name == "DrSetScale" || name == "SetSpriteMoneScale" || name == "DrUiSetScale") {
		if (RuntimeEntity* entity = ent(0)) {
			const float scale = F(1);
			if (plausible_scale(scale)) {
				entity->scale = scale;
				mark_dirty();
			}
		}
		return DispatchResult::Handled;
	}

	// Playback direction / SE / pre-render hints. The renderer has no reverse or
	// per-frame list model yet, so record the direction and accept the rest.
	if (name == "SetSpriteMoneReverse") {
		if (RuntimeEntity* entity = ent(0)) {
			entity->reverse = I(1) != 0;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "SetSpriteMonePreRenderAll") {
		if (RuntimeEntity* entity = ent(0)) {
			entity->pre_render_all = I(1) != 0;
			entity->prerender_force = entity->pre_render_all;
			trace_event(
				"sprite",
				"prerender",
				"\"name\":\"" + trace::quote(entity->name) + "\",\"all\":" + trace::boolean(entity->pre_render_all));
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "SetSpriteMoneStopSE" || name == "SetSpriteMoneDispOffFrameList")
		return DispatchResult::Handled;

	if (name == "GetSpriteMoneFindCallbackTiming") {
		// TODO(anim): callback timing table is not recovered yet.
		gen->SetReturnDWord(0);
		return DispatchResult::Handled;
	}

	if (name == "DrUiSetDirX" || name == "SetSpriteDirX" || name == "DrSetDirX") {
		if (RuntimeEntity* entity = ent(0)) {
			entity->dir_x = I(1) < 0 ? -1.0f : 1.0f;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}

	// DrUiSetColorA(name, value, time): fade the alpha channel to `value` over
	// `time` scene frames.
	if (name == "DrUiSetColorA" || name == "SetSpriteColorA" || name == "DrSetColorA" || name == "SetColorA") {
		if (RuntimeEntity* entity = ent(0)) {
			const int target = I(1) & 0xff;
			const int time = std::max(0, I(2));
			if (time > 0) {
				entity->fade_start = frame;
				entity->fade_duration = time;
				entity->fade_from = entity->alpha;
				entity->fade_to = target;
				entity->alpha = target;
			} else {
				entity->alpha = target;
				entity->fade_duration = 0;
				entity->fade_to = target;
			}
			mark_dirty();
		}
		return DispatchResult::Handled;
	}

	if (name == "DeleteSprite" || name == "DrUiDeleteChara" || name == "DrDeleteEffect" || name == "DeleteEffect" ||
		name == "DeleteObject") {
		// Effects are not entities; record a delete event so the viewer's effect
		// manager can drop the instance.
		if (name == "DrDeleteEffect" || name == "DeleteEffect" || name == "DeleteSprite" || name == "DeleteObject") {
			RuntimeEffect event;
			event.frame = frame;
			event.kind = "delete";
			event.name = S(0);
			_timeline.effects.push_back(event);
		}
		if (RuntimeEntity* entity = ent(0)) {
			const int fade = std::max(0, I(1));
			const int delay = std::max(0, I(2));
			spdlog::debug("delete: {} target={} delay={} fade={}", name, entity->name, delay, fade);
			if (fade > 0) {
				entity->fade_start = frame + delay;
				entity->fade_duration = fade;
				entity->fade_from = entity->alpha;
				entity->fade_to = 0;
			} else if (delay == 0) {
				entity->visible = false;
			} else {
				entity->fade_start = frame + delay;
				entity->fade_duration = 1;
				entity->fade_from = entity->alpha;
				entity->fade_to = 0;
			}
			mark_dirty();
		}
		return DispatchResult::Handled;
	}

	// Whole-screen colour fades: ColorFadeAsync(FnColor, time) ramps the overlay
	// to `color`; CrossFadeAsync(time) is modelled as a dip to black.
	if (name == "ColorFadeAsync" || name == "ColorFade2" || name == "ColorFade" || name == "ColorFade2Async") {
		float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		args.color(0, color);
		push_fade(color, I(1));
		trace_event(
			"stage",
			"fade",
			"\"from\":[" + trace::number(color[0]) + "," + trace::number(color[1]) + "," + trace::number(color[2]) +
				"," + trace::number(color[3]) + "],\"time\":" + trace::integer(I(1)));
		return DispatchResult::Handled;
	}
	if (name == "CrossFadeAsync") {
		const long time = std::max(1L, static_cast<long>(I(0)));
		const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		float from[4];
		if (_timeline.fades.empty()) {
			from[0] = from[1] = from[2] = 0.0f;
			from[3] = 0.0f;
		} else {
			fade_at(frame, from);
		}
		_timeline.fades.push_back(ScreenFade{
			frame, frame + time, {from[0], from[1], from[2], from[3]}, {black[0], black[1], black[2], black[3]}});
		float middle[4];
		fade_at(frame + time, middle);
		_timeline.fades.push_back(ScreenFade{
			frame + time,
			frame + 2 * time,
			{middle[0], middle[1], middle[2], middle[3]},
			{clear[0], clear[1], clear[2], clear[3]}});
		return DispatchResult::Handled;
	}

	if (name == "DrSetLayerMask") {
		// The bit -> MBS/OTBM part mapping is render-side (deferred).
		const unsigned mask = parse_hex(S(1));
		// Verified from DrLib::DrFacial/DrClearFacial bytecode: the flag is
		// `!show` — true clears (hides) the masked layers, false shows them.
		if (RuntimeEntity* entity = ent(0)) {
			if (I(2))
				entity->layer_mask &= ~mask;
			else
				entity->layer_mask |= mask;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrSetVariationType") {
		const unsigned mask = parse_hex(S(0));
		if (I(1))
			_variation |= mask;
		else
			_variation &= ~mask;
		_timeline.variations.push_back(VariationEvent{frame, _variation});
		mark_dirty();
		return DispatchResult::Handled;
	}
	// TODO: src/dst direction assumed.
	if (name == "DrCopyCharaInfo") {
		if (RuntimeEntity* destination = ent(1)) {
			if (RuntimeEntity* source = ent(0)) {
				destination->mbs = source->mbs;
				destination->ftx = source->ftx;
				destination->kind = source->kind;
				mark_dirty();
			}
		}
		return DispatchResult::Handled;
	}
	if (name == "TutorialFadeAsync") {
		float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		args.color(0, color);
		push_fade(color, I(1));
		return DispatchResult::Handled;
	}

	return DispatchResult::Unhandled;
}

} // namespace asb
