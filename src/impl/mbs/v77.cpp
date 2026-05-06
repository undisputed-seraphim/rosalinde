#include "detail.hpp"

#include <array>
#include <cstdint>
#include <istream>
#include <span>
#include <vector>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace mbs {

constexpr auto table_v77 = std::array{
	detail::lookup_entry{0x54, 4, 0xb0, 0x18}, // 0:s0  fog colors
	detail::lookup_entry{0x58, 4, 0xb8, 0x30}, // 1:s1  texture UVs
	detail::lookup_entry{0x5c, 4, 0xc0, 0x30}, // 2:s2  vertices
	detail::lookup_entry{0x60, 4, 0xc8, 0x50}, // 3:s3  hitboxes
	detail::lookup_entry{0x50, 4, 0xd0, 0x14}, // 4:s4  keyframe layers
	detail::lookup_entry{0x64, 2, 0xd8, 0x08}, // 5:s5  hitbox entries
	detail::lookup_entry{0x6a, 2, 0xe0, 0x1c}, // 6:s6  keyframes
	detail::lookup_entry{0x66, 2, 0xe8, 0x24}, // 7:s7  transforms
	detail::lookup_entry{0x68, 2, 0xf0, 0x20}, // 8:s8  animation frames
	detail::lookup_entry{0x6c, 2, 0xf8, 0x30}, // 9:s9  tracks
	detail::lookup_entry{0x6e, 2, 0x100, 0x18}, // a:sa  sequences
	detail::lookup_entry{0x72, 2, 0x108, 0x14}, // b:sb  extras
};

void parse_v77(std::span<const uint8_t> data, v77& v) {
	auto tbl = table_v77;
	byte_reader r(data);
	detail::resolve_table(r, tbl);
	detail::populate_sections(r, v, tbl);
}

std::istream& operator>>(std::istream& is, v77& v) {
	is.seekg(0, std::ios::end);
	const auto size = is.tellg();
	is.seekg(0, std::ios::beg);
	std::vector<uint8_t> buffer(static_cast<size_t>(size));
	is.read(reinterpret_cast<char*>(buffer.data()), size);
	parse_v77(buffer, v);
	return is;
}

glm::mat4 s7_matrix(const section_7& s7, const bool flipx, const bool flipy) {
	const int8_t x = flipx ? -1 : 1;
	const int8_t y = flipy ? -1 : 1;
	glm::mat4 m{1.0};
	m = glm::scale(m, glm::vec3{s7.scale.x * x, s7.scale.y * y, 1.0});
	m = glm::translate(m, glm::vec3{s7.move.x * x, s7.move.y * y, s7.move.z});
	m *= glm::eulerAngleXYZ(s7.rotate.x, s7.rotate.y, s7.rotate.z);
	return m;
}

} // namespace mbs
