#include "mpd.hpp"

#include <cstring>

namespace mpd {

std::optional<file> parse(std::span<const uint8_t> data) {
	if (data.size() < sizeof(header))
		return std::nullopt;

	file f;
	std::memcpy(&f.hdr, data.data(), sizeof(header));
	if (std::memcmp(f.hdr.magic, "MPD\0", 4) != 0)
		return std::nullopt;
	if (f.hdr.header_size != sizeof(header))
		return std::nullopt;
	if (f.hdr.data_size != data.size() - sizeof(header))
		return std::nullopt;
	if (size_t{f.hdr.record_count} > (data.size() - sizeof(header)) / sizeof(record))
		return std::nullopt;
	if (sizeof(header) + size_t{f.hdr.record_count} * sizeof(record) != data.size())
		return std::nullopt;

	f.records.resize(f.hdr.record_count);
	if (!f.records.empty())
		std::memcpy(f.records.data(), data.data() + sizeof(header), f.records.size() * sizeof(record));
	return f;
}

std::optional<file> parse(std::istream& is) {
	std::vector<uint8_t> buffer;
	char c;
	while (is.get(c))
		buffer.push_back(static_cast<uint8_t>(c));
	return parse(std::span<const uint8_t>(buffer.data(), buffer.size()));
}

std::optional<file> parse(std::istream&& is) { return parse(is); }

} // namespace mpd
