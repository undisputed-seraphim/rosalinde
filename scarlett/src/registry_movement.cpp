#include "native_args.hpp"
#include "registry.hpp"

#include <cmath>

namespace asb {

namespace {

int motion_duration_frames(float distance, float speed) {
	if (distance <= 1e-3f || speed <= 1e-3f)
		return 0;
	return std::max(1, static_cast<int>(std::ceil(distance / speed / 2.0f)));
}

} // namespace

DispatchResult Registry::dispatch_movement(std::string_view name, NativeArgs& args) {
	if (name == "Move" || name == "MoveAbs") {
		float to[2] = {0.0f, 0.0f};
		args.vec2(1, to);
		if (RuntimeEntity* entity = entity_arg(args)) {
			Move move{};
			for (int k = 0; k < 2; ++k) {
				move.from[k] = entity->pos[k];
				move.to[k] = name == "Move" ? entity->pos[k] + to[k] : to[k];
			}
			move.from[2] = move.to[2] = entity->pos[2];
			move.f0 = frame;
			move.f1 = frame + std::max(1, args.i32(2));
			_moves[entity->name] = move;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrMoveAbs" || name == "DrMoveAbsAsync" || name == "DrMoveAsync" || name == "DrShortMoveAsync" ||
		name == "DrMove") {
		float to[3] = {0.0f, 0.0f, 0.0f};
		args.vec3(1, to);
		if (RuntimeEntity* entity = entity_arg(args)) {
			Move move{};
			for (int k = 0; k < 3; ++k) {
				move.from[k] = entity->pos[k];
				move.to[k] = to[k];
			}
			int duration = args.i32(4);
			const float dx = move.to[0] - move.from[0];
			const float dy = move.to[1] - move.from[1];
			const float dz = move.to[2] - move.from[2];
			if (const int speed_duration =
					motion_duration_frames(std::sqrt(dx * dx + dy * dy + dz * dz), args.f32(3));
				speed_duration > 0)
				duration = speed_duration;
			if (duration <= 0)
				duration = 30;
			move.f0 = frame;
			move.f1 = frame + duration;
			const std::string animation = args.str(2);
			if (!animation.empty()) {
				int animation_index = -1;
				if (_resolve_track)
					animation_index = _resolve_track(entity->mbs, animation);
				move.animation_token = _animations.begin_motion(*entity, animation, animation_index, frame);
				if (name != "DrShortMoveAsync") {
					move.followup_track = args.str(6);
					if (!move.followup_track.empty() && _resolve_track)
						move.followup_track_index = _resolve_track(entity->mbs, move.followup_track);
				}
			}
			_moves[entity->name] = move;
			mark_dirty();
			if (name == "DrMove") {
				pending_wait = duration;
				if (asIScriptContext* context = asGetActiveContext())
					context->Suspend();
			}
		}
		return DispatchResult::Handled;
	}
	if (name == "MoveWait" || name == "MoveWaitAll") {
		const std::string only = name == "MoveWait" ? args.str(0) : std::string();
		long wait = 0;
		for (const auto& [entity_name, move] : _moves) {
			if (!only.empty() && entity_name != only)
				continue;
			wait = std::max(wait, move.f1 - frame);
		}
		if (wait > 0) {
			pending_wait = wait;
			if (asIScriptContext* context = asGetActiveContext())
				context->Suspend();
		}
		return DispatchResult::Handled;
	}
	if (name == "FollowDir" || name == "DrTurnAsync") {
		if (RuntimeEntity* entity = entity_arg(args)) {
			const auto target = _by_name.find(args.str(1));
			if (target != _by_name.end()) {
				entity->dir_x = _timeline.entities[target->second].pos[0] < entity->pos[0] ? -1.0f : 1.0f;
				mark_dirty();
			}
		}
		return DispatchResult::Handled;
	}
	if (name == "DrMoveAbsByTimeAsync") {
		float to[3] = {0.0f, 0.0f, 0.0f};
		args.vec3(1, to);
		if (RuntimeEntity* entity = entity_arg(args)) {
			Move move{};
			for (int k = 0; k < 3; ++k) {
				move.from[k] = entity->pos[k];
				move.to[k] = to[k];
			}
			move.f0 = frame;
			move.f1 = frame + std::max(1, args.i32(3));
			const std::string animation = args.str(2);
			if (!animation.empty()) {
				int animation_index = -1;
				if (_resolve_track)
					animation_index = _resolve_track(entity->mbs, animation);
				move.animation_token = _animations.begin_motion(*entity, animation, animation_index, frame);
				move.followup_track = args.str(4);
				if (!move.followup_track.empty() && _resolve_track)
					move.followup_track_index = _resolve_track(entity->mbs, move.followup_track);
			}
			_moves[entity->name] = move;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrShortMoveAbsAsync" || name == "DrShortMove") {
		float to[3] = {0.0f, 0.0f, 0.0f};
		args.vec3(1, to);
		if (RuntimeEntity* entity = entity_arg(args)) {
			Move move{};
			float distance2 = 0.0f;
			for (int k = 0; k < 3; ++k) {
				move.from[k] = entity->pos[k];
				move.to[k] = to[k];
				const float delta = to[k] - entity->pos[k];
				distance2 += delta * delta;
			}
			const int speed_duration = motion_duration_frames(std::sqrt(distance2), args.f32(3));
			const int duration = speed_duration > 0 ? speed_duration : 30;
			move.f0 = frame;
			move.f1 = frame + std::max(1, duration);
			const std::string animation = args.str(2);
			if (!animation.empty()) {
				int animation_index = -1;
				if (_resolve_track)
					animation_index = _resolve_track(entity->mbs, animation);
				move.animation_token = _animations.begin_motion(*entity, animation, animation_index, frame);
			}
			_moves[entity->name] = move;
			mark_dirty();
			if (name == "DrShortMove") {
				pending_wait = std::max(1, duration);
				if (asIScriptContext* context = asGetActiveContext())
					context->Suspend();
			}
		}
		return DispatchResult::Handled;
	}
	if (name == "DrJumpAsync" || name == "DrJump") {
		if (RuntimeEntity* entity = entity_arg(args)) {
			const float move_distance = args.f32(1);
			const float height = args.f32(2);
			const float speed = args.f32(4);
			Move move{};
			for (int k = 0; k < 3; ++k) {
				move.from[k] = entity->pos[k];
				move.to[k] = entity->pos[k];
			}
			move.to[0] += entity->dir_x * move_distance;
			const int duration =
				speed > 1e-3f ? static_cast<int>(std::ceil(std::abs(move_distance) / speed)) : 30;
			move.f0 = frame;
			move.f1 = frame + std::max(1, duration);
			move.arc = std::abs(height);
			_moves[entity->name] = move;
			mark_dirty();
			if (name == "DrJump") {
				pending_wait = std::max(1, duration);
				if (asIScriptContext* context = asGetActiveContext())
					context->Suspend();
			}
		}
		return DispatchResult::Handled;
	}
	return DispatchResult::Unhandled;
}

} // namespace asb
