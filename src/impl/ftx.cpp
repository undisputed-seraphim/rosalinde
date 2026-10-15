#include "ftx.hpp"
#include <criware/utils.hpp>

#include <detex.h>

#include <algorithm>
#include <cstring>
#include <spanstream>
#include <vector>

#pragma pack(push, 1)
struct ftex_header {
	char ftex[4];
	uint32_t file_size;
	uint32_t header_size;
	uint32_t count;

	static constexpr std::string_view MAGIC = "FTEX";
};

struct ftex_entry {
	char filename[32];
	uint32_t _pad;
	uint32_t size;
	uint32_t _unk0;
	uint32_t _unk1;
};

struct ftx0_header {
	char ftx0[4];
	uint32_t file_size;
	uint32_t header_size;

	static constexpr std::string_view MAGIC = "FTX0";
};

struct tex_header {
	char _tex[4];
	uint32_t format;
	uint32_t _unk0;
	uint32_t width;
	uint32_t height;
	uint32_t _unk1;
	uint32_t _unk2;
	uint32_t s1;
	uint32_t s2;

	static constexpr std::string_view MAGIC = ".tex";
};
#pragma pack(pop)

namespace FTX {

template <unsigned int Num>
std::istream& discard_bytes(std::istream& i) {
	return i.seekg(Num, std::ios::cur);
}

std::vector<Entry> parse(std::istream& is) { return parse(std::move(is)); }
std::vector<Entry> parse(std::istream&& is) {
	is.seekg(0, std::ios::beg);
	const auto ftex_hdr = read_value<ftex_header>(is);
	if (std::string_view(ftex_hdr.ftex, sizeof(ftex_hdr.ftex)) != ftex_header::MAGIC) {
		return {};
	}

	std::vector<Entry> entries;
	discard_bytes<16>(is);
	for (uint32_t i = 0; i < ftex_hdr.count; ++i) {
		const auto entry = read_value<ftex_entry>(is);
		auto filename = std::string(entry.filename, strlen(entry.filename));
		entries.emplace_back(Entry{std::move(filename), {}, 0, 0, 0, 0, Compression::NONE, Swizzle::NONE});
	}

	is.seekg(ftex_hdr.header_size, std::ios::beg);
	for (auto& entry : entries) {
		const auto curpos = is.tellg();
		const auto ftx0_hdr = read_value<ftx0_header>(is);
		if (std::string_view(ftx0_hdr.ftx0, sizeof(ftx0_hdr.ftx0)) != ftx0_header::MAGIC) {
			return {};
		}
		is.seekg(size_t(curpos) + ftx0_hdr.header_size, std::ios::beg);
		const auto hdr = read_value<tex_header>(is);
		if (std::string_view(hdr._tex, sizeof(hdr._tex)) != tex_header::MAGIC) {
			continue;
		}
		is.seekg(-sizeof(hdr), std::ios::cur);
		entry.width = hdr.width;
		entry.height = hdr.height;
		entry.compressed_size = hdr.s1;
		entry.uncompressed_size = hdr.s2;
		entry.rgba.resize(ftx0_hdr.file_size);
		is.read(reinterpret_cast<char*>(entry.rgba.data()), ftx0_hdr.file_size);

		switch (hdr.format) {
		case 0x44:
			entry.comp = Compression::BC3_UNORM;
			entry.swizzle = Swizzle::TEGRA_X1;
			break;
		case 0x49:
			entry.comp = Compression::BC4_UNORM;
			entry.swizzle = Swizzle::TEGRA_X1;
			break;
		case 0x4d:
			entry.comp = Compression::BC7_UNORM;
			entry.swizzle = Swizzle::TEGRA_X1;
			break;
		default: {
			entry.comp = Compression::NONE;
			entry.swizzle = Swizzle::TEGRA_X1;
		}
		}
	}
	return entries;
}

std::vector<Entry> parse(std::span<char> buffer) { return parse(std::ispanstream(buffer, std::ios::binary)); }

// Forward declarations for decompress
unsigned texture_decompress_bc3(
	unsigned compressed_size,
	unsigned uncompressed_size,
	std::span<const uint8_t> in,
	std::vector<uint8_t>& out);
unsigned texture_decompress_bc4(
	unsigned compressed_size,
	unsigned uncompressed_size,
	std::span<const uint8_t> in,
	std::vector<uint8_t>& out);
unsigned texture_decompress_bc7(
	unsigned compressed_size,
	unsigned uncompressed_size,
	std::span<const uint8_t> in,
	std::vector<uint8_t>& out);

// Forward declarations for deswizzle
static void tegra_x1_deswizzle(unsigned width, unsigned height, std::span<const uint8_t>, std::vector<uint8_t>&);

unsigned decompress(Entry& entry) {
	int ret = 0;
	std::vector<uint8_t> buffer;
	switch (entry.comp) {
	case Compression::BC7_UNORM: {
		ret = texture_decompress_bc7(entry.compressed_size, entry.uncompressed_size, entry.rgba, buffer);
		break;
	}
	case Compression::NONE:
	default:
		break; // no-op
	}
	std::swap(entry.rgba, buffer);
	return ret;
}

unsigned texture_decompress_bc3(
	const unsigned compressed_size,
	const unsigned uncompressed_size,
	std::span<const uint8_t> in,
	std::vector<uint8_t>& out) {
	in = in.last(in.size() - compressed_size);
	out.resize(uncompressed_size * 4);
	for (unsigned i = 0; i < uncompressed_size; i += 8) {
		if (detexDecompressBlockBC3(in.data() + i, DETEX_MODE_MASK_ALL, 0, out.data() + (i * 4))) {
		} else {
			return i;
		}
	}
	return 0;
}

unsigned texture_decompress_bc4(
	const unsigned compressed_size,
	const unsigned uncompressed_size,
	std::span<const uint8_t> in,
	std::vector<uint8_t>& out) {
	in = in.last(in.size() - compressed_size);
	out.resize(uncompressed_size * 4);
	for (unsigned i = 0; i < uncompressed_size; i += 8) {
		if (detexDecompressBlockRGTC1(in.data() + i, DETEX_MODE_MASK_ALL, 0, out.data() + (i * 4))) {
		} else {
			return i;
		}
	}
	return 0;
}

unsigned texture_decompress_bc7(
	const unsigned compressed_size,
	const unsigned uncompressed_size,
	std::span<const uint8_t> in,
	std::vector<uint8_t>& out) {
	in = in.last(in.size() - compressed_size);
	out.resize(uncompressed_size * 4);
	for (unsigned i = 0; i < uncompressed_size; i += 16) {
		if (detexDecompressBlockBPTC(in.data() + i, DETEX_MODE_MASK_ALL_MODES_BPTC, 0, out.data() + (i * 4))) {
		} else {
			return i;
		}
	}
	return 0;
}

void deswizzle(Entry& entry) {
	std::vector<uint8_t> buffer;
	switch (entry.swizzle) {
	case Swizzle::TEGRA_X1:
		tegra_x1_deswizzle(entry.width, entry.height, entry.rgba, buffer);
		break;
	case Swizzle::NONE:
	default:
		// no-op
		break;
	}
	std::swap(entry.rgba, buffer);
}

unsigned block_bytes(Compression c) {
	switch (c) {
	case Compression::BC3_UNORM:
	case Compression::BC3_SRGB:
	case Compression::BC4_UNORM:
	case Compression::BC4_SNORM:
		return 8;
	case Compression::BC7_UNORM:
	case Compression::BC7_SRGB:
		return 16;
	default:
		return 0;
	}
}

namespace {

uint32_t pow2_round_up(uint32_t x) {
	x -= 1;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

// Tegra X1 block-linear (GOB) byte address for block-space coords (see the
// Tegra X1 TRM). `bpp` is the block size in bytes (16 for BC7, 8 for BC3/BC4).
uint32_t gob_addr(uint32_t x, uint32_t y, uint32_t image_width_blocks, uint32_t bpp, uint32_t block_height) {
	const uint32_t iwg = (image_width_blocks * bpp + 63u) / 64u;
	uint32_t a = (y / (8u * block_height)) * 512u * block_height * iwg + (x * bpp / 64u) * 512u * block_height +
				 ((y % (8u * block_height)) / 8u) * 512u;
	const uint32_t xx = x * bpp;
	return a + ((xx % 64u) / 32u) * 256u + ((y % 8u) / 2u) * 64u + ((xx % 32u) / 16u) * 32u + (y % 2u) * 16u +
		   (xx % 16u);
}

} // namespace

static void tegra_x1_deswizzle(
	const unsigned width,
	const unsigned height,
	std::span<const uint8_t> buffer,
	std::vector<uint8_t>& result) {
	if (buffer.empty() || width < 4 || height < 4) {
		result.clear();
		return;
	}

	// The compressed/decoded surface is stored as 4x4 blocks in GOB order; the
	// block grid may be taller than the image (padded). bpp == 16 for the BC7
	// surface layout; the block permutation is identical for the decoded RGBA.
	constexpr uint32_t unit = 64; // one 4x4 RGBA block
	const uint32_t wb = width >> 2;
	const uint32_t himg = height >> 2;
	const uint32_t nsrc = static_cast<uint32_t>(buffer.size() / unit);
	const uint32_t hb = wb ? nsrc / wb : 0;
	const uint32_t bh = std::min(16u, pow2_round_up(hb / 8u));

	result.assign(static_cast<size_t>(width) * height * 4, 0);
	for (uint32_t y = 0; y < hb; ++y) {
		for (uint32_t x = 0; x < wb; ++x) {
			const uint32_t i = gob_addr(x, y, wb, 16, bh) / 16u; // source block index
			if (i >= nsrc || y >= himg)
				continue;
			for (uint32_t r = 0; r < 4; ++r)
				std::memcpy(
					result.data() + (static_cast<size_t>(y * 4 + r) * width + x * 4) * 4,
					buffer.data() + (static_cast<size_t>(i) * 4 + r) * 16,
					16);
		}
	}
}

} // namespace FTX
