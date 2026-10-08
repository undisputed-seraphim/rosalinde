#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct AnimationGraph {
	enum class Action : uint8_t {
		Idle,
		WalkRight,
		WalkLeft,
		Attack,
	};

	struct Phase {
		std::string track_name;
		uint32_t duration_ticks = 0;
		std::string next_track;
	};

	static constexpr float kTicksPerSecond = 60.0f;

	AnimationGraph();

	const Phase* resolve(Action action) const;

	std::pair<const Phase*, bool> tick(float dt_seconds);

	void begin(const Phase& phase);

	const Phase* current_phase() const { return _current; }
	uint32_t elapsed_ticks() const { return _elapsed; }
	float progress() const;

	std::string current_track_name() const;

private:
	const Phase* _current = nullptr;
	uint32_t _elapsed = 0;

	std::unordered_map<Action, std::string> _actions;
	std::unordered_map<std::string, Phase> _chain;

	void ch(const std::string& name, uint32_t dur, const std::string& next = {});
};
