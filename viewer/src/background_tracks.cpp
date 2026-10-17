#include "background_tracks.hpp"

#include <algorithm>
#include <string>

std::vector<uint32_t> select_background_tracks(const mbs::v77& v77) {
	std::vector<uint32_t> enabled;
	for (uint32_t i = 0; i < v77.s9.size(); ++i) {
		const auto& s9 = v77.s9[i];
		if (s9.disabled || v77.sa[s9.sa_set_id].s8_no == 0)
			continue;
		enabled.push_back(i);
	}

	std::vector<uint32_t> day;
	for (uint32_t i : enabled) {
		const std::string n = v77.s9[i].name;
		if (n.find("_yoru") != std::string::npos || n.rfind("yoru", 0) == 0)
			continue;
		day.push_back(i);
	}
	if (!day.empty())
		enabled = day;

	const bool is_state_machine = std::any_of(enabled.begin(), enabled.end(), [&](uint32_t i) {
		const std::string n = v77.s9[i].name;
		return n.find("ENKE") != std::string::npos || n.find("KNKE") != std::string::npos;
	});
	if (is_state_machine) {
		auto pick = [&](const char* needle) -> int {
			for (uint32_t i : enabled) {
				if (std::string(v77.s9[i].name).find(needle) != std::string::npos)
					return static_cast<int>(i);
			}
			return -1;
		};
		int chosen = pick("ENKE_IDLE");
		if (chosen < 0)
			chosen = pick("ENKE");
		if (chosen < 0)
			chosen = pick("VWR_IDLE");
		if (chosen < 0)
			chosen = pick("IDLE");
		if (chosen >= 0)
			return {static_cast<uint32_t>(chosen)};
	}
	return enabled;
}
