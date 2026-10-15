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

enum class Swizzle : char {
	NONE = 0,
	TEGRA_X1,
};

struct Entry {
	std::string name;
	std::vector<uint8_t> rgba;
	unsigned width;
	unsigned height;
	unsigned compressed_size;
	unsigned uncompressed_size;
	Compression comp;
	Swizzle swizzle;
};

std::vector<Entry> parse(std::istream&);
std::vector<Entry> parse(std::istream&&);
std::vector<Entry> parse(std::span<char>);

// BCn compressed textures return 0 if no errors, block ID if an error occurred.
unsigned decompress(Entry&);
void deswizzle(Entry&);

// Bytes per 4x4 block for a compression format (16 for BC7, 8 for BC3/BC4,
// 0 for uncompressed).
unsigned block_bytes(Compression);

} // namespace FTX
