#pragma once

#include <scarlett/scene.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace asb {

struct TrackInfo {
	long duration_frames = 0; // whole sequence, in 60 Hz scene frames
	long action_frames = 0;	  // one-shot intro of the main sequence
	bool one_shot = false;
	int continuation_track = -1;
	std::string continuation_name;
	uint8_t transition_mode = 0;
};

using TrackInfoProvider = std::function<TrackInfo(const std::string& mbs, int track_index)>;

class AnimationController {
public:
	void reset() {
		_states.clear();
		_next_token = 1;
	}

	void set_track_info(TrackInfoProvider provider) { _track_info = std::move(provider); }

	void play(RuntimeEntity& entity, std::string track, int track_index, long frame) {
		apply(entity, std::move(track), track_index, frame);
		auto& state = _states[entity.name];
		state.motion_token = 0;
		state.action_active = false;
		state.motion_end = 0;
		entity.action_active = false;
		entity.action_end_frame = -1;
	}

	void play_index(RuntimeEntity& entity, int track_index, long frame) {
		entity.track_index = track_index;
		entity.animation_start_frame = frame;
		entity.started = true;
		auto& state = _states[entity.name];
		state.motion_token = 0;
		state.action_active = false;
		state.motion_end = 0;
		entity.action_active = false;
		entity.action_end_frame = -1;
	}

	void play_idle(RuntimeEntity& entity, const std::string& track, int track_index, long frame) {
		auto& state = _states[entity.name];
		state.idle_track = track;
		state.idle_index = track_index;
		entity.idle_track = track;
		entity.idle_track_index = track_index;
		if (!state.action_active && state.motion_token == 0)
			apply(entity, track, track_index, frame);
	}

	void play_action(RuntimeEntity& entity, const std::string& track, int track_index, long frame) {
		if (track.empty())
			return;
		auto& state = _states[entity.name];
		if (!state.action_active) {
			state.idle_track = entity.track;
			state.idle_index = entity.track_index;
			entity.idle_track = entity.track;
			entity.idle_track_index = entity.track_index;
		}
		apply(entity, track, track_index, frame);
		const TrackInfo info = info_for(entity, track_index);
		state.action_active = true;
		// An action is the one-shot intro of the clip; the remaining frames loop
		// and are replaced by the idle/follow-up. Waiting for the whole sequence
		// stalls scripts for seconds (D_SIT_A3 is a 31-frame stand-up inside a
		// 152-frame loop).
		const long action = info.action_frames > 0 ? info.action_frames : info.duration_frames;
		state.action_end = frame + std::max(1L, action);
		state.action_continuation = info.one_shot ? info.continuation_track : -1;
		state.action_continuation_name = info.one_shot ? info.continuation_name : std::string();
		entity.action_active = true;
		entity.action_end_frame = state.action_end;
	}

	void set_idle(RuntimeEntity& entity, const std::string& track, int track_index, long frame) {
		auto& state = _states[entity.name];
		state.idle_track = track;
		state.idle_index = track_index;
		entity.idle_track = track;
		entity.idle_track_index = track_index;
		if (!state.action_active && state.motion_token == 0)
			apply(entity, track, track_index, frame);
	}

	uint64_t begin_motion(RuntimeEntity& entity, const std::string& track, int track_index, long frame) {
		if (track.empty())
			return 0;
		auto& state = _states[entity.name];
		if (state.action_active) {
			state.action_active = false;
			state.action_continuation = -1;
			entity.action_active = false;
			entity.action_end_frame = -1;
		} else if (state.idle_track.empty()) {
			state.idle_track = entity.track;
			state.idle_index = entity.track_index;
			entity.idle_track = entity.track;
			entity.idle_track_index = entity.track_index;
		}
		apply(entity, track, track_index, frame);
		const uint64_t token = _next_token++;
		state.motion_token = token;
		const TrackInfo info = info_for(entity, track_index);
		state.motion_end = info.one_shot ? frame + std::max(1L, info.duration_frames) : 0;
		state.motion_continuation = info.one_shot ? info.continuation_track : -1;
		state.motion_continuation_name = info.one_shot ? info.continuation_name : std::string();
		return token;
	}

	void finish_motion(
		RuntimeEntity& entity,
		uint64_t token,
		const std::string& followup,
		int followup_index,
		long frame) {
		if (token == 0)
			return;
		auto it = _states.find(entity.name);
		if (it == _states.end() || it->second.motion_token != token)
			return;
		it->second.motion_token = 0;
		if (!followup.empty())
			apply(entity, followup, followup_index, frame);
		else if (it->second.motion_continuation >= 0)
			apply(
				entity,
				it->second.motion_continuation_name,
				it->second.motion_continuation,
				frame);
		else if (!it->second.idle_track.empty())
			apply(entity, it->second.idle_track, it->second.idle_index, frame);
		it->second.motion_end = 0;
		it->second.motion_continuation = -1;
	}

	long action_remaining(const RuntimeEntity& entity, long frame) const {
		auto it = _states.find(entity.name);
		if (it == _states.end() || !it->second.action_active)
			return 0;
		return std::max(0L, it->second.action_end - frame);
	}

	bool advance(RuntimeEntity& entity, long frame) {
		auto it = _states.find(entity.name);
		if (it == _states.end())
			return false;
		State& state = it->second;
		bool changed = false;
		if (state.action_active && frame >= state.action_end) {
			state.action_active = false;
			entity.action_active = false;
			entity.action_end_frame = -1;
			if (state.action_continuation >= 0) {
				apply(
					entity,
					state.action_continuation_name,
					state.action_continuation,
					frame);
			} else if (!state.idle_track.empty()) {
				apply(entity, state.idle_track, state.idle_index, frame);
			}
			entity.idle_track = state.idle_track;
			entity.idle_track_index = state.idle_index;
			state.action_continuation = -1;
			changed = true;
		}
		if (state.motion_end > 0 && frame >= state.motion_end) {
			if (state.motion_continuation >= 0)
				apply(
					entity,
					state.motion_continuation_name,
					state.motion_continuation,
					frame);
			state.motion_end = 0;
			state.motion_continuation = -1;
			changed = true;
		}
		return changed;
	}

	void clear(const RuntimeEntity& entity) { _states.erase(entity.name); }

private:
	struct State {
		uint64_t motion_token = 0;
		long motion_end = 0;
		int motion_continuation = -1;
		std::string motion_continuation_name;
		bool action_active = false;
		long action_end = 0;
		int action_continuation = -1;
		std::string action_continuation_name;
		std::string idle_track;
		int idle_index = -1;
	};

	TrackInfo info_for(const RuntimeEntity& entity, int track_index) const {
		if (_track_info && track_index >= 0)
			return _track_info(entity.mbs, track_index);
		return {};
	}

	static void apply(RuntimeEntity& entity, std::string track, int track_index, long frame) {
		entity.track = std::move(track);
		entity.track_index = track_index;
		entity.animation_start_frame = frame;
		entity.started = true;
	}

	std::unordered_map<std::string, State> _states;
	TrackInfoProvider _track_info;
	uint64_t _next_token = 1;
};

} // namespace asb
