#include "ftx.hpp"
#include <criware/utils.hpp>

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
		entries.emplace_back(Entry{std::move(filename), {}, 0, 0, 0, 0, Compression::NONE});
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
			break;
		case 0x49:
			entry.comp = Compression::BC4_UNORM;
			break;
		case 0x4d:
			entry.comp = Compression::BC7_UNORM;
			break;
		default:
			entry.comp = Compression::NONE;
			break;
		}
	}
	return entries;
}

std::vector<Entry> parse(std::span<char> buffer) { return parse(std::ispanstream(buffer, std::ios::binary)); }

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

} // namespace FTX
