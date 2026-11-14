#pragma once

#include <scarlett/scene.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
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

	void remove(const std::string& name) { _states.erase(name); }

	void set_track_info(TrackInfoProvider provider) { _track_info = std::move(provider); }

	void play(RuntimeEntity& entity, std::string track, int track_index, long frame) {
		apply(entity, std::move(track), track_index, frame);
		auto& state = _states[entity.name];
		state.motion_token = 0;
		clear_action(state);
		entity.action_active = false;
		entity.action_end_frame = -1;
		state.motion_continuation = -1;
		state.motion_continuation_name.clear();
	}

	void play_index(RuntimeEntity& entity, int track_index, long frame) {
		entity.track_index = track_index;
		entity.animation_start_frame = frame;
		entity.started = true;
		auto& state = _states[entity.name];
		state.motion_token = 0;
		clear_action(state);
		entity.action_active = false;
		entity.action_end_frame = -1;
		state.motion_continuation = -1;
		state.motion_continuation_name.clear();
	}

	// Show a clip without selecting it as the idle/follow-up track (engine turn
	// slot 4: the previous idle must survive the turn). State 0x84 also replaces
	// any queued action, so the action slot is cleared.
	void play_transient(RuntimeEntity& entity, std::string track, int track_index, long frame) {
		auto& state = _states[entity.name];
		clear_action(state);
		entity.action_active = false;
		entity.action_end_frame = -1;
		apply(entity, std::move(track), track_index, frame);
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

	// DrAnimation/DrAnimationAsync queue up to three one-shot animations. The
	// engine stores them in character slots 0/1/2, plays them in order and only
	// then enters the idle state (slot 3, set by DrSetIdleAnimation).
	void queue_actions(
		RuntimeEntity& entity,
		const std::array<std::string, 3>& tracks,
		const std::array<int, 3>& indices,
		long frame) {
		auto& state = _states[entity.name];
		clear_action(state);
		for (size_t i = 0; i < tracks.size(); ++i) {
			if (tracks[i].empty())
				continue;
			ActionStep step;
			step.track = tracks[i];
			step.index = indices[i];
			const TrackInfo info = info_for(entity, step.index);
			const long action = info.action_frames > 0 ? info.action_frames : info.duration_frames;
			step.length = std::max(1L, action);
			state.action_queue[state.action_queue_count++] = std::move(step);
		}
		if (state.action_queue_count == 0) {
			entity.action_active = false;
			entity.action_end_frame = -1;
			return;
		}
		start_action_step(entity, state, 0, frame);
	}

	void play_action(RuntimeEntity& entity, const std::string& track, int track_index, long frame) {
		queue_actions(entity, {track, {}, {}}, {track_index, -1, -1}, frame);
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
			clear_action(state);
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
		// The move's f1 owns the transition; finish_motion applies this. Ending
		// the motion at the clip's own duration cuts walk cycles short.
		state.motion_continuation = info.continuation_track;
		state.motion_continuation_name = info.continuation_name;
		return token;
	}

	void
	finish_motion(RuntimeEntity& entity, uint64_t token, const std::string& followup, int followup_index, long frame) {
		if (token == 0)
			return;
		auto it = _states.find(entity.name);
		if (it == _states.end() || it->second.motion_token != token)
			return;
		it->second.motion_token = 0;
		if (!followup.empty())
			apply(entity, followup, followup_index, frame);
		else if (it->second.motion_continuation >= 0)
			apply(entity, it->second.motion_continuation_name, it->second.motion_continuation, frame);
		else if (!it->second.idle_track.empty())
			apply(entity, it->second.idle_track, it->second.idle_index, frame);
		it->second.motion_continuation = -1;
	}

	long action_remaining(const RuntimeEntity& entity, long frame) const {
		auto it = _states.find(entity.name);
		if (it == _states.end() || !it->second.action_active)
			return 0;
		const State& state = it->second;
		long remaining = std::max(0L, state.action_end - frame);
		for (size_t i = state.action_queue_pos + 1; i < state.action_queue_count; ++i)
			remaining += state.action_queue[i].length;
		return remaining;
	}

	bool advance(RuntimeEntity& entity, long frame) {
		auto it = _states.find(entity.name);
		if (it == _states.end())
			return false;
		State& state = it->second;
		if (!state.action_active || frame < state.action_end)
			return false;
		if (state.action_queue_pos + 1 < state.action_queue_count) {
			start_action_step(entity, state, state.action_queue_pos + 1, frame);
			return true;
		}
		clear_action(state);
		entity.action_active = false;
		entity.action_end_frame = -1;
		// The engine enters state 0x81 after the queue drains and applies the
		// idle slot there. With no idle set, the last action track keeps looping.
		if (!state.idle_track.empty())
			apply(entity, state.idle_track, state.idle_index, frame);
		entity.idle_track = state.idle_track;
		entity.idle_track_index = state.idle_index;
		return true;
	}

	void clear(const RuntimeEntity& entity) { _states.erase(entity.name); }

private:
	struct ActionStep {
		std::string track;
		int index = -1;
		long length = 0;
	};

	struct State {
		uint64_t motion_token = 0;
		int motion_continuation = -1;
		std::string motion_continuation_name;
		bool action_active = false;
		long action_end = 0;
		std::array<ActionStep, 3> action_queue;
		size_t action_queue_pos = 0;
		size_t action_queue_count = 0;
		std::string idle_track;
		int idle_index = -1;
	};

	static void clear_action(State& state) {
		state.action_active = false;
		state.action_end = 0;
		state.action_queue_pos = 0;
		state.action_queue_count = 0;
	}

	void start_action_step(RuntimeEntity& entity, State& state, size_t pos, long frame) {
		state.action_queue_pos = pos;
		const ActionStep& step = state.action_queue[pos];
		apply(entity, step.track, step.index, frame);
		state.action_active = true;
		state.action_end = frame + step.length;
		entity.action_active = true;
		entity.action_end_frame = state.action_end;
	}

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
