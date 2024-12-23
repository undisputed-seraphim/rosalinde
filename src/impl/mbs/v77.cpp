#include "sections.hpp"

#include <unordered_map>
#include <utility>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace MBS_ {

class lol : public Quad {
public:
	void operator()(const v77& v77) {
		get_keyframes_hitboxes_slots(v77);
		get_anims_skels(v77);
	}

private:
	void get_keyframes_hitboxes_slots(const v77&);
	void get_anims_skels(const v77&);
};

Quad v77::to_quad() const {
	lol q;
	q(*this);
	return static_cast<Quad>(q);
}

enum s4flag_mode : uint8_t {
	SKIP = 0x02,
	NOTEX = 0x04,
};

enum s8flag_mode : uint16_t {
	FLIPX = 0x01,
	FLIPY = 0x02,
	JUMP = 0x04,
	//? = 0x20, // Most s8 seem to have this, but doesn't seem to mean anything
	//? = 0x80, // doesn't mean anything
	HITBOX = 0x400,
	LAST = 0x800, // Means ignore this frame apparently
	//? = 0x2000, // Rare
};

static glm::mat4 s7_matrix(const section_7& s7, const bool flipx, const bool flipy) {
	const int8_t x = flipx ? -1 : 1;
	const int8_t y = flipy ? -1 : 1;
	glm::mat4 m{1.0};

	m = glm::scale(m, glm::vec3{
		s7.scale[0] * x,
		s7.scale[1] * y,
		1.0
	});

	m *= glm::eulerAngleXYZ(s7.rotate[0], s7.rotate[1], s7.rotate[2]);

	m = glm::translate(m, glm::vec3{
		s7.move[0] * x,
		s7.move[1] * y,
		s7.move[2]
	});
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

				if (s8v.flags & s8flag_mode::JUMP) {
					j = s8v.loop_s8_id;
				} else {
					if (s8v.flags & s8flag_mode::LAST) {
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

void lol::get_anims_skels(const v77& v77) {
	const auto s8list = s8sa_loop::preprocess(v77);

	_skeletons.reserve(v77.s9.size());
	for (int32_t i = 0; i < v77.s9.size(); ++i) {
		const auto& s9 = v77.s9[i];

		if (s9.sa_set_no < 1) {
			continue;
		}
		auto& skel = _skeletons.emplace_back();
		skel.name.assign(s9.name);
		skel.tracks.reserve(s9.sa_sb_set_no);
		for (uint8_t j = 0; j < s9.sa_set_no; ++j) {
			const int32_t sak = s9.sa_set_id + j;

			auto& anim = skel.tracks.emplace_back();
			anim.id = sak;
			anim.loop_id = s8list[sak].loop;
			anim.bounds = glm::vec4{s9.left, s9.right, s9.top, s9.bottom};

			const auto& s8l = s8list[sak];
			for (const auto& s8 : s8l.times) {
				auto& tl = anim.keyframes.emplace_back();
				tl.attach = [&v77](const uint16_t s6k) {
					const section_6& s6 = v77.s6[s6k];
					if (s6.s4_no > 0 && s6.s5_no > 0) {
						return Attach{s6k, ObjectType::SLOT};
					}
					if (s6.s4_no > 0) {
						return Attach{s6k, ObjectType::KEYFRAME};
					}
					if (s6.s5_no > 0) {
						return Attach{s6k, ObjectType::HITBOX};
					}
					return Attach{s6k, ObjectType::NONE};
				}(s8.s6_id);

				const bool flipx = s8flag_mode::FLIPX & s8.flags;
				const bool flipy = s8flag_mode::FLIPY & s8.flags;
				const auto& s7 = v77.s7[s8.s7_id];
				tl.matrix = s7_matrix(s7, flipx, flipy);
				tl.color = s7.fog;
				tl.time = s8.frames;
				tl.matrix_mix = s8.s7_interpolation;
				tl.color_mix = s8.s7_interpolation;
				tl.keyframe_mix = s8.s6_interpolation;
				tl.hitbox_mix = s8.s5s3_interpolation;
			}
		}
	}
}

void lol::get_keyframes_hitboxes_slots(const v77& v77) {
	_keyframes.resize(v77.s6.size());
	_hitboxes.resize(v77.s6.size());

	for (uint32_t i = 0; i < v77.s6.size(); ++i) {
		const auto& s6 = v77.s6[i];

		// Keyframes
		auto& keyframe = _keyframes[i];
		keyframe.bounds = glm::vec4{s6.left, s6.right, s6.top, s6.bottom};
		keyframe.layers.resize(s6.s4_no);
		for (int j = 0; j < s6.s4_no; ++j) {
			const auto& s4 = v77.s4[s6.s4_id + j];
			auto& layer = keyframe.layers[j];

			if (s4flag_mode::SKIP & s4.flags) {
				continue;
			}

			const auto& s2 = v77.s2[s4.s2_id];
			static_assert(sizeof(layer.dst) == sizeof(s2.values));
			::memcpy(&layer.dst, s2.values, sizeof(layer.dst));
			layer.blendid = s4.blend_id;

			const auto& s0 = v77.s0[s4.s0_id];
			std::memcpy(layer.fog, s0.colors, sizeof(layer.fog));

			if (!(s4flag_mode::NOTEX & s4.flags)) {
				layer.texid = s4.tex_id;
				const auto& s1v = v77.s1[s4.s1_id];
				static_assert(sizeof(layer.src) == sizeof(s1v.values));
				::memcpy(&layer.src, s1v.values, sizeof(layer.src));
			}

			layer.attribute = s4.attributes;
			layer.color = s4.color_id;
		}
		keyframe.id = i;

		// Hitboxes
		auto& hitbox = _hitboxes[i];
		hitbox.layers.resize(s6.s5_no);
		for (uint32_t j = 0; j < s6.s5_no; ++j) {
			const auto& s5 = v77.s5[s6.s5_id + j];
			const auto& s3 = v77.s3[s5.s3_id];
			auto& layer = hitbox.layers[j];
			static_assert(sizeof(layer.hitbox) == sizeof(s3.hitbox));
			::memcpy(&layer.hitbox, &s3.hitbox, sizeof(layer.hitbox));
		}
		hitbox.id = i;
	}
}

} // namespace MBS_