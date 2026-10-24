#include "native_args.hpp"
#include "registry.hpp"

#include <algorithm>
#include <cmath>

namespace asb {

namespace {

bool plausible_scale(float value) { return std::isfinite(value) && value > 1e-4f && value < 1e4f; }

} // namespace

DispatchResult Registry::dispatch_camera(std::string_view name, NativeArgs& args) {
	if (name == "CameraMoveWait")
		return DispatchResult::RecordOnly;
	if (name == "ClearCamera" || name == "DrClearCamera") {
		mark_dirty();
		return DispatchResult::Handled;
	}
	if (name == "GetCameraPosV") {
		if (float* result = static_cast<float*>(args.generic()->GetAddressOfReturnLocation())) {
			result[0] = _timeline.camera.pos[0];
			result[1] = _timeline.camera.pos[1];
		}
		return DispatchResult::Handled;
	}
	if (name == "DrGetCameraPosV") {
		if (float* result = static_cast<float*>(args.generic()->GetAddressOfReturnLocation()))
			for (int k = 0; k < 3; ++k)
				result[k] = _timeline.camera.pos[k];
		return DispatchResult::Handled;
	}
	if (name == "DrSetCameraPosV" || name == "SetCameraPosV" || name == "SetCameraTargetPosV" ||
		name == "SetCameraTargetPosVBySpeed") {
		float position[3] = {0.0f, 0.0f, 0.0f};
		args.vec3(0, position);
		auto& camera = _timeline.camera;
		const long duration = std::max(0L, static_cast<long>(args.i32(1)));
		const long delay = std::max(0L, static_cast<long>(args.i32(2)));
		camera.has_pos = true;
		if (name == "DrSetCameraPosV" && duration > 0) {
			camera.tween.start_frame = frame + delay;
			camera.tween.end_frame = camera.tween.start_frame + duration;
			camera.tween.start_pos[0] = camera.pos[0];
			camera.tween.start_pos[1] = camera.pos[1];
			camera.tween.start_pos[2] = camera.pos[2];
			const float zoom = camera.has_zoom && camera.zoom > 0.0f ? camera.zoom : 1.0f;
			// FUN_0016e88c: target = pos + origin/zoom. Only the camera's base
			// offset is scaled; the script position is already in view units.
			for (int k = 0; k < 3; ++k)
				camera.tween.target_pos[k] = position[k] + camera.origin[k] / zoom;
			camera.tween.active = true;
			set_camera_ease(camera.tween, duration);
			camera.pos[0] = camera.tween.start_pos[0];
			camera.pos[1] = camera.tween.start_pos[1];
			camera.pos[2] = camera.tween.start_pos[2];
			_camera_move_end = camera.tween.end_frame;
		} else {
			for (int k = 0; k < 3; ++k)
				camera.pos[k] = position[k];
			camera.tween.active = false;
			camera.tween.zoom_active = false;
			_camera_move_end = frame;
		}
		trace_event(
			"camera",
			"pos",
			"\"pos\":" + trace::vec3(position) + ",\"duration\":" + trace::integer(duration) +
				",\"delay\":" + trace::integer(delay) + ",\"end\":" + trace::integer(camera.tween.end_frame) +
				",\"target\":" + trace::vec3(duration > 0 ? camera.tween.target_pos : position));
		mark_dirty();
		return DispatchResult::Handled;
	}
	if (name == "DrSetCameraZoom") {
		const float zoom = args.f32(0);
		if (plausible_scale(zoom)) {
			auto& camera = _timeline.camera;
			const long duration = std::max(0L, static_cast<long>(args.i32(1)));
			const long delay = std::max(0L, static_cast<long>(args.i32(2)));
			if (duration > 0) {
				camera.tween.start_frame = frame + delay;
				camera.tween.end_frame = camera.tween.start_frame + duration;
				camera.tween.start_zoom = camera.has_zoom && camera.zoom > 0.0f ? camera.zoom : 1.0f;
				camera.tween.target_zoom = zoom;
				camera.tween.zoom_active = true;
				set_camera_ease(camera.tween, duration);
				camera.zoom = camera.tween.start_zoom;
				camera.has_zoom = true;
			} else {
				camera.zoom = zoom;
				camera.has_zoom = true;
				camera.tween.zoom_active = false;
			}
			trace_event(
				"camera",
				"zoom",
				"\"zoom\":" + trace::number(zoom) + ",\"duration\":" + trace::integer(duration) +
					",\"delay\":" + trace::integer(delay));
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrSetCameraParam" || name == "SetCameraParam") {
		_timeline.camera.accel = args.f32(0);
		_timeline.camera.decel = args.f32(1);
		_timeline.camera.param_set = true;
		trace_event(
			"camera",
			"param",
			"\"accel\":" + trace::number(_timeline.camera.accel) +
				",\"decel\":" + trace::number(_timeline.camera.decel));
		mark_dirty();
		return DispatchResult::Handled;
	}
	if (name == "DrResetCameraParam") {
		_timeline.camera.accel = 0.0f;
		_timeline.camera.decel = 0.0f;
		_timeline.camera.param_set = false;
		mark_dirty();
		return DispatchResult::Handled;
	}
	if (name == "ResetCameraParam") {
		_timeline.camera.accel = _timeline.camera.decel = 0.0f;
		mark_dirty();
		return DispatchResult::Handled;
	}
	if (name == "DrTebureCamera") {
		_timeline.camera.tebure = args.i32(0) != 0;
		mark_dirty();
		return DispatchResult::Handled;
	}
	if (name == "DrFollowCamera" || name == "FollowCamera" || name == "FollowCameraBySpeed") {
		if (RuntimeEntity* entity = entity_arg(args)) {
			for (int k = 0; k < 3; ++k)
				_timeline.camera.pos[k] = entity->pos[k];
			_timeline.camera.has_pos = true;
			if (name == "DrFollowCamera") {
				_timeline.camera.tween.active = false;
				_timeline.camera.tween.zoom_active = false;
			}
			mark_dirty();
		}
		const long wait = std::max(0, args.i32(2));
		if (wait > 0) {
			pending_wait = wait;
			if (asIScriptContext* context = asGetActiveContext())
				context->Suspend();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrCameraMoveWait") {
		if (_camera_move_end > frame) {
			pending_wait = _camera_move_end - frame;
			if (asIScriptContext* context = asGetActiveContext())
				context->Suspend();
		}
		return DispatchResult::Handled;
	}
	if (name == "SetDemoCameraPosY") {
		// TODO(demo-camera): the value is an absolute Y (FlDemoCameraStart); it is
		// applied to the recorded camera, but the engine may layer it differently.
		_timeline.camera.demo_y = args.f32(0);
		_timeline.camera.pos[1] = args.f32(0);
		mark_dirty();
		return DispatchResult::Handled;
	}
	if (name == "ResetDemoCameraPosY") {
		_timeline.camera.demo_y = 0.0f;
		mark_dirty();
		return DispatchResult::Handled;
	}
	if (name == "ResetDemoCameraPosXZ") {
		_timeline.camera.demo_xz[0] = _timeline.camera.demo_xz[1] = 0.0f;
		mark_dirty();
		return DispatchResult::Handled;
	}
	if (name == "SetCameraDegreeOffset") {
		_timeline.camera.degree_offset = args.f32(0);
		mark_dirty();
		return DispatchResult::Handled;
	}
	if (name == "FollowOffsetV") {
		args.vec2(0, _follow_offset);
		return DispatchResult::Handled;
	}
	if (name == "DrCameraStop") {
		_timeline.camera.tween.active = false;
		_timeline.camera.tween.zoom_active = false;
		_camera_move_end = frame;
		return DispatchResult::Handled;
	}
	if (name == "DrWaveCamera") {
		_timeline.camera.wave = args.i32(0) != 0;
		mark_dirty();
		return DispatchResult::Handled;
	}
	if (name == "DrResetCameraZoomParam")
		return DispatchResult::RecordOnly;
	return DispatchResult::Unhandled;
}

} // namespace asb
