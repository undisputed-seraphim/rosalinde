#include "native_args.hpp"
#include "registry.hpp"

#include <algorithm>
#include <cmath>

namespace asb {

namespace {

bool plausible_scale(float value) {
	return std::isfinite(value) && value > 1e-4f && value < 1e4f;
}

} // namespace

DispatchResult Registry::dispatch_camera(std::string_view name, NativeArgs& args) {
	if (name == "CameraMoveWait")
		return DispatchResult::RecordOnly;
	if (name == "ClearCamera" || name == "DrClearCamera") {
		mark_dirty();
		return DispatchResult::Handled;
	}
	if (name == "GetCameraPosV") {
		float* result = static_cast<float*>(args.generic()->GetAddressOfReturnLocation());
		if (result) {
			result[0] = _timeline.camera.pos[0];
			result[1] = _timeline.camera.pos[1];
		}
		return DispatchResult::Handled;
	}
	if (name == "DrGetCameraPosV") {
		float* result = static_cast<float*>(args.generic()->GetAddressOfReturnLocation());
		if (result)
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
			for (int k = 0; k < 3; ++k)
				camera.tween.target_pos[k] = position[k] / zoom + camera.origin[k];
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
	return DispatchResult::Unhandled;
}

} // namespace asb
