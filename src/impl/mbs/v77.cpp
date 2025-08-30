#include "sections.hpp"

#include <unordered_map>
#include <utility>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace mbs {

enum s4flag : uint8_t {
	SKIP = 0x02,
	NOTEX = 0x04,
};

enum s8flag : uint32_t {
	// clang-format off
	FLIPX = 0x01,   // 0b ----'----'----'---1
	FLIPY = 0x02,   // 0b ----'----'----'--1-
	JUMP = 0x04,    // 0b ----'----'----'-1-- // Something to do with the loops?
	//? = 0x20,     // 0b ----'----'--1-'---- // Most s8 seem to have this, but doesn't seem to mean anything
	//? = 0x80,     // 0b ----'----'1---'---- // doesn't mean anything
	HITBOX = 0x400, // 0b ----'-1--'----'----
	LAST = 0x800,	// 0b ----'1---'----'---- // Means ignore this frame apparently
	//? = 0x2000,   // 0b --1-'----'----'---- // Ignore next?
	// clang-format on
};

static glm::mat4 s7_matrix(const section_7& s7, const bool flipx, const bool flipy) {
	const int8_t x = flipx ? -1 : 1;
	const int8_t y = flipy ? -1 : 1;
	glm::mat4 m{1.0};
	m = glm::scale(m, glm::vec3{s7.scale.x * x, s7.scale.y * y, 1.0});
	m = glm::translate(m, glm::vec3{s7.move.x * x, s7.move.y * y, s7.move.z});
	m *= glm::eulerAngleXYZ(s7.rotate.x, s7.rotate.y, s7.rotate.z);
	// NOTE: Normally the right order for this is scale-rotate-translate,
	// however accessories seem to be wrongly placed.
	// So scale-translate-rotate appears to get us closest to the right image.
	// I think there is some parent-child transform hierarchy that's currently missing.
	return m;
}
/*
struct s8sa_loop {
	int loop;
	std::vector<section_8> times;

	static std::vector<s8sa_loop> preprocess(const v77&);
};

std::vector<s8sa_loop> s8sa_loop::preprocess(const v77& v77) {
	std::vector<s8sa_loop> s8sa_loops;
	s8sa_loops.reserve(v77.sa.size());
	for (const auto& a : v77.sa) {
		const int sav = a.s8_id + a.s8_st;
		auto& [loop, times] = s8sa_loops.emplace_back();
		loop = -1;

		std::unordered_map<int, int> line;
		for (int j = 0, s8k = sav; s8k < v77.s8.size(); s8k = sav + j) {
			const auto& s8v = v77.s8[s8k];
			if (line.count(s8k) == 0) {
				line[s8k] = line.size();
				times.push_back(s8v);

				if (s8v.flags & s8flag::JUMP) {
					j = s8v.loop_s8_id;
				} else {
					if (s8v.flags & s8flag::LAST) {
						break;
					} else {
						j++;
					}
				}
			} else {
				loop = line[s8k];
				break;
			}
		}
	}
	return s8sa_loops;
}
*/

} // namespace mbs