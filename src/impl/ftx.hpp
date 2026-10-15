#pragma once

#include <istream>
#include <span>
#include <string>
#include <vector>

namespace FTX {

enum class Compression : char {
	NONE = 0,

	// BC3 textures
	BC3_UNORM,
	BC3_SRGB,

	// BC4 textures
	BC4_UNORM,
	BC4_SNORM,

	// BC7 textures
	BC7_UNORM,
	BC7_SRGB,
};

struct Entry {
	std::string name;
	std::vector<uint8_t> rgba;
	unsigned width;
	unsigned height;
	unsigned compressed_size;
	unsigned uncompressed_size;
	Compression comp;
};

std::vector<Entry> parse(std::istream&);
std::vector<Entry> parse(std::istream&&);
std::vector<Entry> parse(std::span<char>);

// Bytes per 4x4 block for a compression format (16 for BC7, 8 for BC3/BC4,
// 0 for uncompressed).
unsigned block_bytes(Compression);

} // namespace FTX
