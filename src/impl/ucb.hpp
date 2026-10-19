#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <optional>
#include <span>
#include <vector>

// UCB field grid (FieldBG/Field.ucb). The file has no header; it is a dense
// row-major grid of 32-byte cells. In Field.ucb the grid is 2199x1311
// (2,882,889 cells).
//
// UCB is the baked form of the engine's spatial object grid for UFB. The engine
// builds that grid from UFB's object list with 32-world-unit cells, then copies
// UCB into it when the cell counts match (FUN_001c3ed0). UCB's width and height
// therefore come from the UFB object bounds, not from the 138x105 UFB terrain
// cell grid.
//
//   0x00  u32 x              grid x, 0 .. width-1
//   0x04  u32 y              grid y, 0 .. height-1
//   0x08  u32 value          packed occupancy entry (see below)
//   0x0c  u32 flags          0xff000000 in every Field.ucb cell
//   0x10  u8[16] reserved    zero in the baked file
//
// index = y * width + x. The value word packs three fields:
//
//   bits  0..15  entry_index   0..1646 in Field.ucb; UFB header count_54 is 1647
//   bit      16  occupied      set for every non-empty cell
//   bits 17..21  zero
//   bits 22..31  object_index  1..266 in Field.ucb; 0 means empty
//
// The engine writes object_index into bits 22..31 and the low entry index into
// bits 0..15, so these are not arbitrary halves. An empty cell has value 0.
// The trailing 16 bytes are runtime flags in the live grid and are zero in UCB.
namespace ucb {

#pragma pack(push, 1)
struct cell {
	uint32_t x;
	uint32_t y;
	uint32_t value;
	uint32_t flags;
	std::array<uint8_t, 16> reserved;

	uint32_t entry_index() const noexcept { return value & 0xffff; }
	bool occupied() const noexcept { return (value & 0x10000) != 0; }
	uint32_t object_index() const noexcept { return value >> 22; }
	uint32_t reserved_bits() const noexcept { return (value >> 17) & 0x1f; }
};
#pragma pack(pop)

static_assert(sizeof(cell) == 32);

inline constexpr uint32_t cell_size = 32;

struct file {
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<cell> cells;
};

std::optional<file> parse(std::span<const uint8_t> data);
std::optional<file> parse(std::istream& is);
std::optional<file> parse(std::istream&& is);

} // namespace ucb
