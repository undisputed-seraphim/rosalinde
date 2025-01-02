#include "sections.hpp"

#include <unordered_map>
#include <utility>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace mbs {

class lol : public Quad {
public:
	void operator()(const v77& v77) {
		get_animations(v77);
		get_keyframes(v77);
		get_keyframe_layers(v77);
		get_hitbox(v77);
	}

private:
	void get_animations(const v77&);
	void get_keyframes(const v77&);
	void get_keyframe_layers(const v77&);
	void get_hitbox(const v77&);
};

Quad v77::to_quad() const {
	lol q;
	q(*this);
	return static_cast<Quad>(q);
}

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
	return m;
}

namespace {

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

} // anonymous namespace

void lol::get_animations(const v77& v77) {
	const auto s8list = s8sa_loop::preprocess(v77);

	_animations.reserve(v77.s9.size());
	for (int32_t i = 0; i < v77.s9.size(); ++i) {
		const auto& s9 = v77.s9[i];

		if (s9.disabled) {
			continue;
		}
		auto& [name, tracks] = _animations.emplace_back();
		name.assign(s9.name);
		tracks.reserve(s9.sa_set_no);
		for (uint8_t j = 0; j < s9.sa_set_no; ++j) {
			const int32_t sak = s9.sa_set_id + j;

			auto& anim = tracks.emplace_back();
			anim.loop_id = s8list[sak].loop;
			anim.bounds = glm::vec4{s9.left, s9.right, s9.top, s9.bottom};

			const auto& s8l = s8list[sak];
			for (const auto& s8 : s8l.times) {
				auto& tl = anim.keyframes.emplace_back();

				tl.keyframe_id = s8.s6_id;

				const bool flipx = s8flag::FLIPX & s8.flags;
				const bool flipy = s8flag::FLIPY & s8.flags;
				const auto& s7 = v77.s7[s8.s7_id];
				tl.matrix = s7_matrix(s7, flipx, flipy);
				tl.color = s7.fog;
				tl.frames = s8.frames;
				tl.kf_interpolation = s8.s6_interpolation;
			}
		}
	}
	_animations.shrink_to_fit();
}

void lol::get_keyframes(const v77& v77) {
	_keyframes.resize(v77.s6.size());
	for (uint32_t i = 0; i < v77.s6.size(); ++i) {
		const auto& s6 = v77.s6[i];

		auto& keyframe = _keyframes[i];
		keyframe.bounds = glm::vec4{s6.left, s6.right, s6.top, s6.bottom};
		for (int j = s6.s4_id; j < s6.s4_id + s6.s4_no; ++j) {
			keyframe.layers.push_back(j);
		}
		for (short j = s6.s5_id; j < s6.s5_id + s6.s5_no; ++j) {
			keyframe.hitboxes.push_back(j);
		}
	}
	_keyframes.shrink_to_fit();
}

void lol::get_keyframe_layers(const v77& v77) {
	_layers.reserve(v77.s4.size());
	for (const auto& s4 : v77.s4) {
		auto& layer = _layers.emplace_back();

		const auto& s0 = v77.s0[s4.s0_id];
		static_assert(sizeof(layer.fog) == sizeof(s0.colors));
		std::memcpy(layer.fog, s0.colors, sizeof(layer.fog));

		if (!(s4flag::NOTEX & s4.flags)) {
			const auto& s1v = v77.s1[s4.s1_id];
			static_assert(sizeof(layer.src) == sizeof(s1v.values));
			std::memcpy(&layer.src, s1v.values, sizeof(layer.src));
		}

		const auto& s2 = v77.s2[s4.s2_id];
		static_assert(sizeof(layer.dst) == sizeof(s2.values));
		std::memcpy(&layer.dst, s2.values, sizeof(layer.dst));

		layer.colorid = s4.color_id;
		layer.flags = s4.flags;
		layer.blendid = s4.blend_id;
		layer.texid = s4.tex_id;
		layer.attributes = s4.attributes;
	}
	_layers.shrink_to_fit();
}

void lol::get_hitbox(const v77& v77) {
	_hitboxes.reserve(v77.s3.size());
	for (const auto& s5 : v77.s5) {
		const auto& s3 = v77.s3[s5.s3_id];
		static_assert(sizeof(glm::mat4x2) == sizeof(s3.hitbox));
		std::memcpy(glm::value_ptr(_hitboxes.emplace_back()), s3.hitbox, sizeof(s3.hitbox));
	}
	_hitboxes.shrink_to_fit();
}

} // namespace mbs