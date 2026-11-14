#include "native_args.hpp"
#include "registry.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace asb {

DispatchResult Registry::dispatch_animation(std::string_view name, NativeArgs& args) {
	if (name == "DrAnimation" || name == "DrAnimationAsync") {
		// DrAnimation (0x31b600) queues the action and then parks on wait type
		// 0x5b until the character's action state is 0x81 (finished); its
		// predicate is 0x31b970. DrAnimationAsync (0x31b850) only queues and
		// returns. Scripts therefore rely on DrAnimation itself to cover the
		// one-shot intro, without a following DrAnimationWait.
		const bool wait_for_action = name == "DrAnimation";
		long remaining = 0;
		std::array<std::string, 3> tracks{args.str(1), args.str(2), args.str(3)};
		std::array<int, 3> indices{-1, -1, -1};
		if (RuntimeEntity* entity = entity_arg(args)) {
			for (size_t i = 0; i < tracks.size(); ++i) {
				if (tracks[i].empty())
					continue;
				if (_resolve_track)
					indices[i] = _resolve_track(entity->mbs, tracks[i]);
				spdlog::debug(
					"anim: {} action {} index={} slot={} frame={}", entity->name, tracks[i], indices[i], i, frame);
			}
			_animations.queue_actions(*entity, tracks, indices, frame);
			if (wait_for_action)
				remaining = _animations.action_remaining(*entity, frame);
			mark_dirty();
			if (remaining > 0) {
				pending_wait = remaining;
				spdlog::debug(
					"wait: DrAnimation {} action={} end={} remaining={}",
					entity->name,
					tracks[0],
					entity->action_end_frame,
					remaining);
				if (asIScriptContext* context = asGetActiveContext())
					context->Suspend();
			}
		}
		return DispatchResult::Handled;
	}
	if (name == "DrSetIdleAnimation") {
		if (RuntimeEntity* entity = entity_arg(args)) {
			const std::string animation = args.str(1);
			int animation_index = -1;
			if (_resolve_track)
				animation_index = _resolve_track(entity->mbs, animation);
			_animations.set_idle(*entity, animation, animation_index, frame);
			spdlog::debug(
				"anim: {} idle {} index={} deferred={} frame={}",
				entity->name,
				animation,
				animation_index,
				entity->action_active,
				frame);
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrActionWait") {
		// DrActionWait (0x31e550 -> 0x17079c) parks the script on a dedicated
		// action wait and never consults the clip length. The action is the
		// one-shot intro of the sequence (the rest loops), so the wait is
		// bounded by that intro: D_SIT_A3 is 51 ticks, not its 303-tick loop.
		// A DrMove*Async with an animation also queues an action: scripts call
		// DrActionWait right after the move to wait for the motion to finish.
		if (RuntimeEntity* entity = entity_arg(args)) {
			long remaining = _animations.action_remaining(*entity, frame);
			if (auto it = _moves.find(entity->name); it != _moves.end() && it->second.animation_token != 0)
				remaining = std::max(remaining, std::max(0L, it->second.f1 - frame));
			if (remaining > 0) {
				pending_wait = remaining;
				spdlog::debug("wait: action {} end={} remaining={}", entity->name, entity->action_end_frame, remaining);
				if (asIScriptContext* context = asGetActiveContext())
					context->Suspend();
			}
		}
		return DispatchResult::Handled;
	}
	if (name == "DrTurn") {
		if (RuntimeEntity* entity = entity_arg(args)) {
			set_animation(*entity, args.str(1));
			mark_dirty();
			const long duration = _track_frames ? _track_frames(entity->mbs, entity->track_index) : 0;
			if (duration > 0) {
				pending_wait = duration;
				if (asIScriptContext* context = asGetActiveContext())
					context->Suspend();
			}
		}
		return DispatchResult::Handled;
	}
	if (name == "SetSpriteMoneAnimation" || name == "SetSpriteMoneAnimationPaused") {
		if (RuntimeEntity* entity = entity_arg(args)) {
			set_animation_index(*entity, args.i32(1));
			entity->anim_paused = name == "SetSpriteMoneAnimationPaused" ? args.i32(2) != 0 : false;
			trace_event(
				"sprite",
				"anim",
				"\"name\":\"" + trace::quote(entity->name) + "\",\"track\":" + trace::integer(entity->track_index) +
					",\"mbs\":\"" + trace::quote(entity->mbs) + "\",\"paused\":" + trace::boolean(entity->anim_paused));
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrUiSetAnimation") {
		if (RuntimeEntity* entity = entity_arg(args)) {
			set_animation_index(*entity, args.i32(1));
			spdlog::debug("anim: {} track={}", entity->name, entity->track_index);
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "WaitSpriteMoneAnimationEnd" || name == "DrAnimationWait") {
		// Wait until a sprite's animation finishes. Only modelled when a length
		// provider knows the track; otherwise it stays immediate. The provider
		// reports a full track duration in 60 Hz scene frames, so subtract the
		// frames already played since the track was selected.
		long total = 0;
		long remaining = 0;
		RuntimeEntity* entity = entity_arg(args);
		if (entity && _track_frames)
			total = _track_frames(entity->mbs, entity->track_index);
		if (entity && total > 0)
			remaining = std::max(0L, total - std::max(0L, frame - entity->animation_start_frame));
		spdlog::debug(
			"wait: animation end name={} track={} mbs={} start={} total={} remaining={} end={}",
			entity ? entity->name : "?",
			entity ? entity->track_index : -1,
			entity ? entity->mbs : "",
			entity ? entity->animation_start_frame : -1,
			total,
			remaining,
			frame + remaining);
		if (remaining > 0) {
			pending_wait = remaining;
			if (asIScriptContext* context = asGetActiveContext())
				context->Suspend();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrSetBreathOff") {
		if (RuntimeEntity* entity = entity_arg(args)) {
			entity->breath_off = args.i32(1) != 0;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrSetLipSync") {
		if (RuntimeEntity* entity = entity_arg(args)) {
			entity->lipsync = args.i32(1) != 0;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	return DispatchResult::Unhandled;
}

} // namespace asb
