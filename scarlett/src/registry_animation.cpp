#include "native_args.hpp"
#include "registry.hpp"

#include <spdlog/spdlog.h>

namespace asb {

DispatchResult Registry::dispatch_animation(std::string_view name, NativeArgs& args) {
	if (name == "DrAnimation" || name == "DrAnimationAsync") {
		if (RuntimeEntity* entity = entity_arg(args)) {
			const std::string animation = args.str(1);
			if (!animation.empty()) {
				int animation_index = -1;
				if (_resolve_track)
					animation_index = _resolve_track(entity->mbs, animation);
				_animations.play_action(*entity, animation, animation_index, frame);
				spdlog::debug(
					"anim: {} action {} index={} start={} end={}",
					entity->name,
					entity->track,
					entity->track_index,
					frame,
					entity->action_end_frame);
			}
			mark_dirty();
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
		if (RuntimeEntity* entity = entity_arg(args)) {
			const long remaining = _animations.action_remaining(*entity, frame);
			if (remaining > 0) {
				pending_wait = remaining;
				spdlog::debug("wait: action {} end={}", entity->name, entity->action_end_frame);
				if (asIScriptContext* context = asGetActiveContext())
					context->Suspend();
			}
		}
		return DispatchResult::Handled;
	}
	return DispatchResult::Unhandled;
}

} // namespace asb
