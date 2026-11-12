#pragma once

namespace asb {

struct MessageTiming {
	int voice_wait_min = 90;
	int voice_off_per_char = 2;
	int voice_off_min = 30;
	int voice_on_extra = 20;
	int sprite_per_char = 7;
	int select_wait = 80;
};

} // namespace asb
