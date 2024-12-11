#include "sections.hpp"

#include <unordered_map>
#include <utility>

#include <glm/ext.hpp>
#include <glm/glm.hpp>

namespace MBS_ {

namespace {

enum s4flag_mode : uint8_t {
	SKIP,
	TEX,
};

enum s8flag_mode : uint8_t {
	LAST,
	JUMP,
	FLIPX,
	FLIPY,
};

uint8_t s4flag_set(s4flag_mode f, uint8_t flag) {
	switch (f) {
	case s4flag_mode::SKIP: {
		return (flag & 0x02);
	}
	case s4flag_mode::TEX: {
		return (flag & 0x04) == 0;
	}
	}
	return 0;
}

uint32_t s8flag_set(s8flag_mode f, uint32_t flag) {
	switch (f) {
	case s8flag_mode::LAST: {
		return (flag & 0x800);
	}
	case s8flag_mode::JUMP: {
		return (flag & 0x04);
	}
	case s8flag_mode::FLIPX: {
		return (flag & 0x01);
	}
	case s8flag_mode::FLIPY: {
		return (flag & 0x02);
	}
	}
	return 0;
}

struct s8sa_loop {
	int loop;
	std::vector<section_8> times;

	static std::vector<s8sa_loop> preprocess(const v77&);
};

std::vector<s8sa_loop> s8sa_loop::preprocess(const v77& v77) {
	struct s8_sa {
		int32_t id;
		int32_t nr;
		uint32_t sum;
	};
	const std::vector<s8_sa> s8sa = [](const std::vector<section_a>& sa) {
		std::vector<s8_sa> out;
		out.reserve(sa.size());
		for (const auto& i : sa) {
			out.emplace_back(s8_sa{int32_t(i.s8_id) + i.s8_st, int32_t(i.s8_no) - i.s8_st, i.s8_sum});
		}
		return out;
	}(v77.sa);

	std::vector<s8sa_loop> s8sa_loops(s8sa.size());
	for (uint32_t i = 0; i < s8sa.size(); ++i) {
		const auto& sav = s8sa[i];
		auto& [loop, time] = s8sa_loops[i];
		loop = -1;

		int32_t j = 0;
		std::unordered_map<int, int> line;
		while (true) {
			const int32_t s8k = sav.id + j;
			if (v77.s8.size() < s8k) {
				break;
			}
			const auto& s8v = v77.s8[s8k];

			if (line.count(s8k) == 0) {
				line[s8k] = line.size();
				time.push_back(s8v);

				if (s8flag_set(s8flag_mode::JUMP, s8v.flags)) {
					j = s8v.loop_s8_id;
				} else {
					if (s8flag_set(s8flag_mode::LAST, s8v.flags)) {
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

glm::mat4 s7_matrix(const section_7& s7, const bool flipx, const bool flipy) {
	const int8_t x = flipx ? -1 : 1;
	const int8_t y = flipy ? -1 : 1;
	glm::mat4 m{1};

	// Translate
	m[0][3] += s7.move[0] * x;
	m[1][3] += s7.move[1] * y;
	m[2][3] += s7.move[2];

	// Rotate
	m *= glm::rotate(m, s7.rotate[2], {0, 0, 1});
	m *= glm::rotate(m, s7.rotate[1], {0, 1, 0});
	m *= glm::rotate(m, s7.rotate[0], {1, 0, 0});

	// Scale
	m[0][0] = s7.scale[0] * x;
	m[1][1] = s7.scale[1] * y;
	return m;
}

glm::vec4 rgba_uint32_to_float4(const uint32_t rgba) {
	// clang-format off
	return (glm::vec4(
		static_cast<float>((rgba >> 24) & 0xFF),
		static_cast<float>((rgba >> 16) & 0xFF),
		static_cast<float>((rgba >>  8) & 0xFF),
		static_cast<float>((rgba >>  0) & 0xFF)
	) / 255.0f);
	// clang-format on
}

void lol::get_anims_skels(const v77& v77) {
	const auto salist = s8sa_loop::preprocess(v77);

	_animations.reserve(v77.s9.size());
	_skeletons.reserve(v77.s9.size());
	for (int32_t i = 0; i < v77.s9.size(); ++i) {
		const auto& s9 = v77.s9[i];

		if (s9.sa_set_no < 1) {
			continue;
		}
		auto& skel = _skeletons.emplace_back(Skeleton{std::string(s9.name, ::strlen(s9.name)), {}});
		for (uint8_t j = 0; j < s9.sa_set_no; ++j) {
			int32_t sak = s9.sa_set_id + j;

			auto& anim = _animations.emplace_back(Animation{});
			anim.id = sak;
			anim.loop_id = salist[sak].loop;

			auto& bone = skel.bones.emplace_back(Skeleton::Bone{});
			bone.id = sak;
			bone.attach = Attach{sak, ObjectType::ANIMATION};

			for (const auto& s8 : salist[sak].times) {
				auto& tl = anim.timelines.emplace_back(Animation::Timeline{});
				tl.attach = [](const section_6& s6, uint16_t s6k) {
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
				}(v77.s6[s8.s6_id], s8.s6_id);

				const bool flipx = s8flag_set(s8flag_mode::FLIPX, s8.flags);
				const bool flipy = s8flag_set(s8flag_mode::FLIPY, s8.flags);
				const auto s7k = s8.s7_id;
				tl.matrix = s7_matrix(v77.s7[s7k], flipx, flipy);
				tl.color = v77.s7[s7k].fog;
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
	_slots.resize(v77.s6.size());

	for (uint32_t i = 0; i < v77.s6.size(); ++i) {
		const auto& s6 = v77.s6[i];

		// Keyframes
		auto& keyframe = _keyframes[i];
		keyframe.layers.resize(s6.s4_no);
		for (int j = 0; j < s6.s4_no; ++j) {
			const auto& s4 = v77.s4[s6.s4_id + j];
			auto& layer = keyframe.layers[j];

			if (s4flag_set(s4flag_mode::SKIP, s4.flags)) {
				continue;
			}

			const auto& s2 = v77.s2[s4.s2_id];
			static_assert(sizeof(layer.dst) == sizeof(s2.values));
			::memcpy(&layer.dst, s2.values, sizeof(layer.dst));
			layer.blendid = s4.blend_id;

			const auto& s0 = v77.s0[s4.s0_id];
			layer.fog = {
				rgba_uint32_to_float4(s0.colors[0]),
				rgba_uint32_to_float4(s0.colors[1]),
				rgba_uint32_to_float4(s0.colors[2]),
				rgba_uint32_to_float4(s0.colors[3]),
			};

			if (s4flag_set(s4flag_mode::TEX, s4.flags)) {
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

		// Slots
		// Section 6 holds keyframe layers and hitboxes.
		// But if both are empty, then it is a placeholder that references
		// other keyframes and hitboxes.
		auto& slot = _slots[i];
		if (s6.s4_no > 0 && s6.s5_no > 0) {
			slot.attachments.push_back(Quad::Attach{(int32_t)i, Quad::ObjectType::KEYFRAME});
			slot.attachments.push_back(Quad::Attach{(int32_t)i, Quad::ObjectType::HITBOX});
		}
	}
}

} // namespace MBS_