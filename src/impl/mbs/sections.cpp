#include "sections.hpp"
#include "../../utils.hpp"
#include <array>
#include <ostream>

namespace MBS_ {

struct lookup_entry {
	int16_t c;			// offset location offset
	int16_t c_size;		// offset value size
	int16_t p;			// offset location offset 2
	int16_t row_length; // row length
	uint32_t h = 0;		// Num entries
	uint32_t offset = 0;
	uint32_t data_size = 0;
};

constexpr auto table_v77 = std::array{
	lookup_entry{0x54, 4, 0xb0, 0x18},	// 1:s0
	lookup_entry{0x58, 4, 0xb8, 0x30},	// 2:s1
	lookup_entry{0x5c, 4, 0xc0, 0x30},	// 3:s2
	lookup_entry{0x60, 4, 0xc8, 0x50},	// 4:s3 bg=0
	lookup_entry{0x50, 4, 0xd0, 0x14},	// 0:s4
	lookup_entry{0x64, 2, 0xd8, 0x8},	// 5:s5 bg=0
	lookup_entry{0x6a, 2, 0xe0, 0x1c},	// 8:s6
	lookup_entry{0x66, 2, 0xe8, 0x24},	// 6:s7
	lookup_entry{0x68, 2, 0xf0, 0x20},	// 7:s8
	lookup_entry{0x6c, 2, 0xf8, 0x30},	// 9:s9
	lookup_entry{0x6e, 2, 0x100, 0x18}, // a:sa
	lookup_entry{0x72, 2, 0x108, 0x14}, // b:sb
};

template <typename T>
std::vector<T> read_block(const lookup_entry& e, std::istream& is) {
	if (e.c == 0 || e.p == 0) {
		return {};
	}
	std::vector<T> entries;
	entries.reserve(e.h);
	is.seekg(e.offset, std::ios::beg);
	for (uint32_t i = 0; i < e.h; ++i) {
		entries.push_back(read_value<T>(is));
	}
	return entries;
}

v77 v77::read(std::istream& is) {
	auto table = table_v77; // Make a copy
	for (auto& [c, cs, p, rl, h, o, ds] : table) {
		if (c == 0 || p == 0) {
			continue;
		}
		is.seekg(c, std::ios::beg);
		switch (cs) {
		case 2: {
			h = read_value<uint16_t>(is);
			break;
		}
		case 4: {
			h = read_value<uint32_t>(is);
			break;
		}
		}
		is.seekg(p, std::ios::beg);
		o = read_value<uint32_t>(is);
		ds = h * rl;
	}
	// Do NOT sort or change the order of table entries!

	return v77{
		read_block<section_0>(table[0x0], is),
		read_block<section_1>(table[0x1], is),
		read_block<section_2>(table[0x2], is),
		read_block<section_3>(table[0x3], is),
		read_block<section_4>(table[0x4], is),
		read_block<section_5>(table[0x5], is),
		read_block<section_6>(table[0x6], is),
		read_block<section_7>(table[0x7], is),
		read_block<section_8>(table[0x8], is),
		read_block<section_9>(table[0x9], is),
		read_block<section_a>(table[0xa], is),
		read_block<section_b>(table[0xb], is)};
}

std::ostream& operator<<(std::ostream& os, const v77& v) {
	os << v.s0.size() << ' ' << v.s1.size() << ' ' << v.s2.size() << ' ' << v.s3.size() << ' ' << v.s4.size() << ' '
	   << v.s5.size() << ' ' << v.s6.size() << ' ' << v.s7.size() << ' ' << v.s8.size() << ' ' << v.s9.size() << ' '
	   << v.sa.size() << ' ' << v.sb.size() << '\n';
	return os;
}

} // namespace MBS_