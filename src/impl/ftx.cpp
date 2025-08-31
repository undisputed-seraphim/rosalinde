#include "ftx.hpp"
#include "utils.hpp"

#include <detex.h>

#include <array>
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
	case Swizzle::NONE:
	default:
		// no-op
		break;
	}
	std::swap(entry.rgba, buffer);
}

void tegra_x1_deswizzle(
	const unsigned width,
	const unsigned height,
	std::span<const uint8_t> buffer,
	std::vector<uint8_t>& result) {
	if (buffer.empty()) {
		return;
	}
	// clang-format off
	static constexpr auto bits = std::array{
		std::array{    0x40,    0x32,     0xd},
		std::array{   0x100,    0xd2,    0x2d},
		std::array{   0x400,   0x392,    0x6d},
		std::array{  0x1000,   0xf12,    0xed},
		std::array{  0x4000,  0x3e12,   0x1ed},
		std::array{ 0x10000,  0x7e12,  0x81ed},
		std::array{ 0x40000,  0xfe12, 0x301ed},
		std::array{0x100000, 0x1fe12, 0xe01ed},
	};
	// clang-format on

	auto swizzle_bitmask = [](uint32_t i, uint32_t mask) -> uint32_t {
		uint32_t bit = 0;
		uint32_t sll = 0;
		while (true) {
			if (mask < 1)
				break;
			if (i < 1)
				break;
			auto bi = i & 1;
			auto bm = mask & 1;
			i >>= 1;
			mask >>= 1;
			if (bm) {
				bit |= (bi << sll);
				sll++;
			}
		}
		return bit;
	};

	result.resize(buffer.size());
	const struct {
		char magic[4];
		uint32_t width;
		uint32_t height;
	} hdr{{'R', 'G', 'B', 'A'}, width, height};
	constexpr uint32_t hdr_offset = sizeof(hdr);
	::memcpy(result.data(), (char*)&hdr, sizeof(hdr));

	constexpr uint32_t bpp = 4; // RBGA
	constexpr uint32_t row = 4 * bpp;
	const uint32_t len_pix = buffer.size();
	const uint32_t len_blk = len_pix >> 6;
	const uint32_t w = (width >> 2);
	const uint32_t h = (height >> 2);
	for (const auto bit : bits) {
		if (len_blk <= bit[0]) {
			uint32_t pos = 0;
			for (int i = 0; i < bit[0] && pos < len_pix; ++i) {
				const auto x = swizzle_bitmask(i, bit[1]);
				const auto y = swizzle_bitmask(i, bit[2]);
				if (x >= w || y >= h) {
					break;
				}

				for (uint32_t z = 0; z < 4; ++z) {
					const uint32_t dyy = (y * 4 + z) * w * 4;
					const uint32_t dxx = x * 4 + dyy;
					std::memcpy(result.data() + (dxx * bpp) + hdr_offset, buffer.data() + pos, row);
					pos += row;
				}
			}
			break;
		}
	}
}

} // namespace FTX
