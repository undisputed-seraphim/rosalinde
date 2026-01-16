#include "sections.hpp"

#include <array>
#include <fstream>

namespace mbs {

namespace ver76 {

struct lookup_entry {
	int16_t c;			// offset location offset
	int16_t c_size;		// offset value size
	int16_t p;			// offset location offset 2
	int16_t row_length; // row length
	uint32_t h = 0;		// Num entries
	uint32_t offset = 0;
	uint32_t data_size = 0;
};

}

constexpr auto table_v76 = std::array{
	ver76::lookup_entry{0x54, 2, 0xb0, 0x18},	// s0
	ver76::lookup_entry{0x56, 2, 0xb8, 0x30},	// s1
	ver76::lookup_entry{0x58, 2, 0xc0, 0x30},	// s2
	ver76::lookup_entry{0x5a, 2, 0xc8, 0x50},	// s3 bg=0
	ver76::lookup_entry{0x50, 4, 0xd0, 0x14},	// s4
	ver76::lookup_entry{0x5c, 2, 0xd8, 0x8},	// s5 bg=0
	ver76::lookup_entry{0x62, 2, 0xe0, 0x1c},	// s6
	ver76::lookup_entry{0x5e, 2, 0xe8, 0x24},	// s7
	ver76::lookup_entry{0x60, 2, 0xf0, 0x20},	// s8
	ver76::lookup_entry{0x64, 2, 0xf8, 0x30},	// s9
	ver76::lookup_entry{0x66, 2, 0x100, 0x18},	// sa
	ver76::lookup_entry{0x6a, 2, 0x108, 0x14},	// sb bg=0
};

} // namespace mbs