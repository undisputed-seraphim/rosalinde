#include "screenshot.hpp"

#include <cstdio>
#include <vector>
#include <zlib.h>

namespace {

void put_u32(std::vector<uint8_t>& out, uint32_t v) {
	out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
	out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
	out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
	out.push_back(static_cast<uint8_t>(v & 0xFF));
}

void put_chunk(std::vector<uint8_t>& out, const char type[4], std::span<const uint8_t> data) {
	put_u32(out, static_cast<uint32_t>(data.size()));
	const size_t crc_start = out.size();
	out.insert(out.end(), type, type + 4);
	out.insert(out.end(), data.begin(), data.end());
	uLong crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, out.data() + crc_start, static_cast<uInt>(out.size() - crc_start));
	put_u32(out, static_cast<uint32_t>(crc));
}

bool write_bytes(const std::filesystem::path& path, std::span<const uint8_t> bytes) {
	FILE* f = fopen(path.string().c_str(), "wb");
	if (!f)
		return false;
	const size_t written = fwrite(bytes.data(), 1, bytes.size(), f);
	fclose(f);
	return written == bytes.size();
}

} // namespace

bool screenshot::write_png(
	const std::filesystem::path& path,
	uint32_t width,
	uint32_t height,
	std::span<const uint8_t> rgba) {
	const size_t stride = static_cast<size_t>(width) * 3;
	std::vector<uint8_t> raw;
	raw.reserve((stride + 1) * height);
	for (uint32_t y = 0; y < height; ++y) {
		raw.push_back(0); // filter: none
		const uint8_t* row = rgba.data() + static_cast<size_t>(height - 1 - y) * width * 4;
		for (uint32_t x = 0; x < width; ++x) {
			raw.push_back(row[x * 4 + 0]);
			raw.push_back(row[x * 4 + 1]);
			raw.push_back(row[x * 4 + 2]);
		}
	}

	uLongf comp_size = compressBound(static_cast<uLong>(raw.size()));
	std::vector<uint8_t> comp(comp_size);
	if (compress2(comp.data(), &comp_size, raw.data(), static_cast<uLong>(raw.size()), Z_BEST_SPEED) != Z_OK)
		return false;
	comp.resize(comp_size);

	std::vector<uint8_t> out;
	static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
	out.insert(out.end(), sig, sig + 8);

	std::vector<uint8_t> ihdr;
	put_u32(ihdr, width);
	put_u32(ihdr, height);
	ihdr.push_back(8); // bit depth
	ihdr.push_back(2); // color type: RGB
	ihdr.push_back(0); // compression
	ihdr.push_back(0); // filter
	ihdr.push_back(0); // interlace
	put_chunk(out, "IHDR", ihdr);
	put_chunk(out, "IDAT", comp);
	put_chunk(out, "IEND", {});

	return write_bytes(path, out);
}

bool screenshot::write_ppm(
	const std::filesystem::path& path,
	uint32_t width,
	uint32_t height,
	std::span<const uint8_t> rgba) {
	FILE* f = fopen(path.string().c_str(), "wb");
	if (!f)
		return false;
	fprintf(f, "P6\n%u %u\n255\n", width, height);
	for (uint32_t y = 0; y < height; ++y) {
		const uint8_t* row = rgba.data() + static_cast<size_t>(height - 1 - y) * width * 4;
		for (uint32_t x = 0; x < width; ++x) {
			fwrite(&row[x * 4], 1, 3, f);
		}
	}
	fclose(f);
	return true;
}

bool screenshot::write(
	const std::filesystem::path& path,
	uint32_t width,
	uint32_t height,
	std::span<const uint8_t> rgba) {
	const auto ext = path.extension().string();
	if (ext == ".ppm" || ext == ".pnm")
		return write_ppm(path, width, height, rgba);
	return write_png(path, width, height, rgba);
}
