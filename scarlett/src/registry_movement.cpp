#include "native_args.hpp"
#include "registry.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace asb {

namespace {

int motion_duration_frames(float distance, float speed) {
	if (distance <= 1e-3f || speed <= 1e-3f)
		return 0;
	return std::max(1, static_cast<int>(std::ceil(distance / speed / 2.0f)));
}

int vector_components(asIScriptGeneric* gen, int index) {
	asITypeInfo* type = gen->GetEngine()->GetTypeInfoById(gen->GetArgTypeId(index));
	const char* name = type ? type->GetName() : nullptr;
	if (!name)
		return 0;
	if (std::strcmp(name, "FnVector2") == 0)
		return 2;
	if (std::strcmp(name, "FnVector3") == 0)
		return 3;
	if (std::strcmp(name, "FnVector4") == 0)
		return 4;
	return 0;
}

} // namespace

DispatchResult Registry::dispatch_movement(std::string_view name, NativeArgs& args) {
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
			if (const int speed_duration = motion_duration_frames(std::sqrt(dx * dx + dy * dy + dz * dz), args.f32(3));
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
			const int duration = speed > 1e-3f ? static_cast<int>(std::ceil(std::abs(move_distance) / speed)) : 30;
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
	if (name == "FollowDirClear") {
		if (RuntimeEntity* e = ent(0)) {
			e->dir_x = 1.0f;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}

	// TODO(move): MoveBack semantics approximated as a negative relative delta;
	// the rest are absolute. `ByTime` variants block.
	auto move2 = [&](int pos_arg, int time_arg, bool absolute, bool negative) -> DispatchResult {
		float to[3] = {0.0f, 0.0f, 0.0f};
		V(pos_arg, to, 2);
		if (RuntimeEntity* e = ent(0)) {
			Move m{};
			for (int k = 0; k < 3; ++k) {
				m.from[k] = e->pos[k];
				m.to[k] = absolute ? to[k] : e->pos[k] + (negative ? -to[k] : to[k]);
			}
			m.to[2] = e->pos[2];
			m.f0 = frame;
			m.f1 = frame + std::max(1, I(time_arg));
			_moves[e->name] = m;
			mark_dirty();
			pending_wait = m.f1 - frame;
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return DispatchResult::Handled;
	};
	if (name == "MoveAbsByTime" || name == "MoveBackAbsByTime")
		return move2(1, 2, true, false);
	if (name == "MoveBackAbs")
		return move2(1, 2, true, false);
	if (name == "MoveBack")
		return move2(1, 2, false, true);
	if (name == "DrSetPosV" || name == "SetSpritePosV" || name == "SetPosV" || name == "DrUiSetPosV" ||
		name == "DrSetOffsetPosV") {
		if (RuntimeEntity* e = ent(0)) {
			float p[3] = {0.0f, 0.0f, 0.0f};
			const int components = vector_components(args.generic(), 1);
			V(1, p, components > 0 ? std::min(components, 3) : 3);
			const int time = name == "DrUiSetPosV" ? std::max(0, I(2)) : 0;
			if (time > 0) {
				Move m{};
				for (int k = 0; k < 3; ++k) {
					m.from[k] = e->pos[k];
					m.to[k] = p[k];
				}
				m.f0 = frame;
				m.f1 = frame + time;
				_moves[e->name] = m;
			} else {
				_moves.erase(e->name);
				e->pos[0] = p[0];
				e->pos[1] = p[1];
				e->pos[2] = p[2];
			}
			mark_dirty();
		}
		return DispatchResult::Handled;
	}

	if (name == "SetUnitDirAsync") {
		if (RuntimeEntity* entity = entity_arg(args)) {
			entity->dir_x = args.i32(1) >= 0 ? 1.0f : -1.0f;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	return DispatchResult::Unhandled;
}

} // namespace asb
