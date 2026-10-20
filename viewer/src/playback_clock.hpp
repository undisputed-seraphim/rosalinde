#pragma once

class PlaybackClock {
public:
	static constexpr float frame_seconds = 1.0f / 60.0f;

	void advance(float seconds) { _elapsed += seconds; }
	float elapsed() const { return _elapsed; }

private:
	float _elapsed = 0.0f;
};
