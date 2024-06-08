#include "ftx.hpp"
#include "bc7decomp/bc7decomp.h"
#include "utils.hpp"

#include <array>
#include <iostream>
#include <spanstream>
#include <vector>

#pragma pack(push, 1)
struct ftex_header {
	char magic[4];
	uint32_t file_size;
	uint32_t header_size;
	uint32_t count;

	static constexpr std::string_view FTX0 = "FTEX";
};

struct ftex_entry {
	char filename[32];
	uint32_t _pad;
	uint32_t size;
	uint32_t _unk0;
	uint32_t _unk1;
};

struct ftx0_header {
	char magic[4];
	uint32_t file_size;
	uint32_t header_size;

	static constexpr std::string_view FTX0 = "FTX0";
};

struct tex_header {
	char magic[4];
	uint32_t format;
	uint32_t _unk0;
	uint32_t width;
	uint32_t height;
	uint32_t _unk1;
	uint32_t _unk2;
	uint32_t s1;
	uint32_t s2;

	static constexpr std::string_view _TEX = ".tex";
};
#pragma pack(pop)

// Forward declarations
// Returns 0 if no error.
int im_bc3();
int im_bc4();
int im_bc7(const tex_header&, std::span<char>, std::vector<char>&);
void tegra_x1_swizzle(const tex_header&, std::vector<char>&);

FTX::FTX() noexcept {}
FTX::FTX(FTX&& other) noexcept
	: _entries({}) {
	std::swap(_entries, other._entries);
}

FTX::iterator FTX::begin() { return _entries.begin(); }
FTX::iterator FTX::end() { return _entries.end(); }
FTX::const_iterator FTX::begin() const { return _entries.begin(); }
FTX::const_iterator FTX::end() const { return _entries.end(); }
FTX::const_iterator FTX::cbegin() const { return _entries.cend(); }
FTX::const_iterator FTX::cend() const { return _entries.cend(); }

FTX::size_type FTX::size() const noexcept { return _entries.size(); }
bool FTX::empty() const noexcept { return _entries.empty(); }

bool FTX::parse(std::istream& is) { return parse(std::move(is)); }
bool FTX::parse(std::istream&& is) {
	is.seekg(0, std::ios::beg);
	const auto ftex_hdr = read_value<ftex_header>(is);
	if (std::string_view(ftex_hdr.magic, sizeof(ftex_hdr.magic)) != ftex_header::FTX0) {
		return false;
	}

	_entries.clear();
	discard_bytes<16>(is);
	for (uint32_t i = 0; i < ftex_hdr.count; ++i) {
		const auto entry = read_value<ftex_entry>(is);
		auto filename = std::string(entry.filename, strlen(entry.filename));
		_entries.emplace_back(Entry{std::move(filename), 0, 0});
	}

	is.seekg(ftex_hdr.header_size, std::ios::beg);
	for (uint32_t i = 0; i < ftex_hdr.count; ++i) {
		const auto curpos = is.tellg();
		const auto ftx0_hdr = read_value<ftx0_header>(is);
		if (std::string_view(ftx0_hdr.magic, sizeof(ftx0_hdr.magic)) != ftx0_header::FTX0) {
			return false;
		}
		is.seekg(size_t(curpos) + ftx0_hdr.header_size, std::ios::beg);
		_entries[i].offset = is.tellg();
		_entries[i].size = ftx0_hdr.file_size;
		_entries[i].rgba.resize(ftx0_hdr.file_size);
		is.seekg(ftx0_hdr.file_size, std::ios::cur);
	}

	for (auto& entry : _entries) {
		entry.rgba.resize(entry.size);
		is.seekg(entry.offset, std::ios::beg);
		is.read(entry.rgba.data(), entry.size);
		is.seekg(entry.offset, std::ios::beg);
		const auto hdr = read_value<tex_header>(is);
		if (std::string_view(hdr.magic, sizeof(hdr.magic)) != tex_header::_TEX) {
			continue;
		}
		std::cout << entry.name << "\tw: " << hdr.width << " h: " << hdr.height << " s1: " << hdr.s1
				  << " s2: " << hdr.s2 << std::endl;
		entry.width = hdr.width;
		entry.height = hdr.height;

		switch (hdr.format) {
		case 0x44:
			im_bc3();
			break;
		case 0x49:
			im_bc4();
			break;
		case 0x4d:
			im_bc7(hdr, entry.rgba, _buffer);
			tegra_x1_swizzle(hdr, _buffer);
			break;
		default: {
			std::cout << "Unsupported format" << std::endl;
			return false;
		}
		}
		if (!_buffer.empty()) {
			std::swap(entry.rgba, _buffer);
		}
	}
	return true;
}

int im_bc3() {
	// TODO: Same as DXT4, DXT5
	return 0;
}

int im_bc4() {
	// TODO: In D3D10
	return 0;
}

int im_bc7(const tex_header& header, std::span<char> buffer, std::vector<char>& outbuf) {
	buffer = buffer.last(buffer.size() - header.s1);
	outbuf.resize(header.s2 * 4);
	for (int i = 0; i < header.s2; i += 16) {
		bc7decomp::color_rgba* pos = reinterpret_cast<bc7decomp::color_rgba*>(outbuf.data() + (i * 4));
		if (bc7decomp::unpack_bc7(buffer.data() + i, pos)) {
			// OK, continue
		} else {
			outbuf.resize(0);
			return i;
		}
	}
	return 0;
}

void tegra_x1_swizzle(const tex_header& header, std::vector<char>& buffer) {
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

	constexpr const char magic[4] = {'R', 'G', 'B', 'A'};
	constexpr uint32_t hdr_offset = sizeof(magic) + sizeof(header.width) + sizeof(header.height);

	std::vector<char> result(buffer.size());
	auto oss = std::ospanstream(result, std::ios::binary);
	oss.write(magic, sizeof(magic));
	oss.write((char*)&header.width, sizeof(header.width));
	oss.write((char*)&header.height, sizeof(header.height));

	constexpr uint32_t bpp = 4; // RBGA
	constexpr uint32_t row = 4 * bpp;
	const uint32_t len_pix = buffer.size();
	const uint32_t len_blk = len_pix >> 6;
	const uint32_t w = (header.width >> 2);
	const uint32_t h = (header.height >> 2);
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
	buffer = result;
}
