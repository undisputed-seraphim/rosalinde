#include "animation_graph.hpp"

AnimationGraph::AnimationGraph() {
	_actions = {
		{Action::Idle, "IDLE"},
		{Action::WalkRight, "D_WALK_A1"},
		{Action::WalkLeft, "D_WALK_D1"},
		{Action::Attack, "MAGIC_A_1"},
	};

	ch("MAGIC_A_1", 186, "MAGIC_A_2");
	ch("MAGIC_A_2", 186, "MAGIC_A_3");
	ch("MAGIC_A_3", 210, "MAGIC_A_4");
	ch("MAGIC_A_4", 171, "MAGIC_A_END");
	ch("MAGIC_A_END", 171);
}

void AnimationGraph::ch(const std::string& name, uint32_t dur, const std::string& next) {
	_chain[name] = {name, dur, next};
}

const AnimationGraph::Phase* AnimationGraph::resolve(Action action) const {
	auto it = _actions.find(action);
	if (it == _actions.end())
		return nullptr;
	auto ci = _chain.find(it->second);
	if (ci == _chain.end())
		return nullptr;
	return &ci->second;
}

void AnimationGraph::begin(const Phase& phase) {
	_current = &phase;
	_elapsed = 0;
}

std::pair<const AnimationGraph::Phase*, bool> AnimationGraph::tick(float dt_seconds) {
	if (!_current)
		return {nullptr, false};

	_elapsed += static_cast<uint32_t>(dt_seconds * kTicksPerSecond);

	if (_elapsed >= _current->duration_ticks) {
		if (!_current->next_track.empty()) {
			auto ci = _chain.find(_current->next_track);
			if (ci != _chain.end()) {
				_current = &ci->second;
				_elapsed = 0;
				return {_current, true};
			}
		}
		_current = nullptr;
		return {nullptr, true};
	}

	return {_current, false};
}

float AnimationGraph::progress() const {
	if (!_current || _current->duration_ticks == 0)
		return 1.0f;
	return static_cast<float>(_elapsed) / static_cast<float>(_current->duration_ticks);
}

std::string AnimationGraph::current_track_name() const { return _current ? _current->track_name : "IDLE"; }
