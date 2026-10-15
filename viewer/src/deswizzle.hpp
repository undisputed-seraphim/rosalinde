#pragma once

#include <cstdint>
#include <span>

namespace FTX {
enum class Compression : char;
}

// GPU stage: deswizzles a Tegra-X1 texture (BCn block stream, or swizzled RGBA)
// and decodes it into a GL_RGBA8 texture-array slice. Replaces the CPU
// decompress + deswizzle. Requires a current GL 4.5 context on construction.
class Deswizzler {
public:
	Deswizzler();
	~Deswizzler();
	Deswizzler(const Deswizzler&) = delete;
	Deswizzler& operator=(const Deswizzler&) = delete;
	Deswizzler(Deswizzler&&) = delete;

	// Write one texture (width x height) into layer `layer` of `array_tex` (a
	// GL_TEXTURE_2D_ARRAY with GL_RGBA8 storage). `payload` is the raw FTX
	// texture data: the BCn block stream (compressed sizes) or the swizzled
	// RGBA data (NONE).
	void upload_slice(
		unsigned array_tex,
		unsigned layer,
		unsigned width,
		unsigned height,
		FTX::Compression comp,
		std::span<const uint8_t> payload);

private:
	void ensure_buffers(size_t bytes);

	unsigned _deswizzle = 0;
	unsigned _expand = 0;
	unsigned _in = 0;
	unsigned _out = 0;
	unsigned _tmp = 0;
	size_t _tmp_capacity = 0;
};
