#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace asb {

// Persistent game progression. Scene entities, playheads, UI state, and battle
// simulation remain in SceneState/Registry; this struct survives scene changes
// and is shared by sessions that belong to the same playthrough.
struct SaveState {
	int gold = 0;
	int fame = 0;

	std::unordered_set<std::string> party;
	std::unordered_map<int, int> slots;
	std::unordered_set<std::string> class_changed;
	std::unordered_set<std::string> disabled;
	std::unordered_map<int, int> quests; // quest id -> bits (1 open, 2 completed)
	std::unordered_set<int> units;		 // recruited unit ids

	std::unordered_map<int, int> stage_flags;
	std::unordered_set<int> stage_bits;
	std::unordered_map<int, int> global_flags;

	std::unordered_map<std::string, int> items;
	std::unordered_set<std::string> facilities;
	std::unordered_map<std::string, int> friendship;
	std::unordered_set<int> treasures;
	std::unordered_set<int> quest_pinned;
	std::unordered_set<int> quest_preopened;
	std::unordered_map<std::string, int> town_lv;

	int clock_hour = 0;
	int clock_minute = 0;
	int clock_second = 0;
	bool clock_paused = false;

	bool balcony_bru = false;
	bool balcony_scr = false;
};

} // namespace asb
