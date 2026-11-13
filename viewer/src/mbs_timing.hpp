#pragma once

namespace mbs {

// Section-8 frame durations are animation ticks. The engine advances them at
// 120 ticks/s while script time and scene frames run at 60 Hz. The opening
// reference pins the rate: a 556-tick logo sequence ends at 4.6 s.
inline constexpr long animation_ticks_per_second = 120;
inline constexpr long scene_frames_per_second = 60;
inline constexpr long animation_ticks_per_frame = animation_ticks_per_second / scene_frames_per_second;

// Tick count -> 60 Hz scene frames, rounded up so a script wait never expires
// before the visible clip finishes.
inline long frames_from_ticks(long ticks) {
	return ticks <= 0 ? 0 : (ticks + animation_ticks_per_frame - 1) / animation_ticks_per_frame;
}

} // namespace mbs
