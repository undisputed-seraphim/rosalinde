#pragma once

#include <scarlett/scene.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace asb {

class AnimationController {
public:
	void reset() {
		_states.clear();
		_next_token = 1;
	}

	void play(RuntimeEntity& entity, std::string track, int track_index, long frame) {
		apply(entity, std::move(track), track_index, frame);
		_states[entity.name].motion_token = 0;
	}

	void play_index(RuntimeEntity& entity, int track_index, long frame) {
		entity.track_index = track_index;
		entity.animation_start_frame = frame;
		entity.started = true;
		_states[entity.name].motion_token = 0;
	}

	uint64_t begin_motion(RuntimeEntity& entity, const std::string& track, int track_index, long frame) {
		if (track.empty())
			return 0;
		apply(entity, track, track_index, frame);
		const uint64_t token = _next_token++;
		_states[entity.name].motion_token = token;
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
	}

	void clear(const RuntimeEntity& entity) { _states.erase(entity.name); }

private:
	struct State {
		uint64_t motion_token = 0;
	};

	static void apply(RuntimeEntity& entity, std::string track, int track_index, long frame) {
		entity.track = std::move(track);
		entity.track_index = track_index;
		entity.animation_start_frame = frame;
		entity.started = true;
	}

	std::unordered_map<std::string, State> _states;
	uint64_t _next_token = 1;
};

} // namespace asb
