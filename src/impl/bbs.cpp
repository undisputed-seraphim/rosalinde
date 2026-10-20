#include "bbs.hpp"

#include <cstring>

namespace bbs {

namespace {

template <typename T>
void read_records(std::span<const uint8_t> data, size_t& offset, size_t count, std::vector<T>& out) {
	out.resize(count);
	if (count != 0)
		std::memcpy(out.data(), data.data() + offset, count * sizeof(T));
	offset += count * sizeof(T);
}

} // namespace

std::optional<file> parse(std::span<const uint8_t> data) {
	if (data.size() < names_offset)
		return std::nullopt;

	file f;
	std::memcpy(&f.hdr, data.data(), sizeof(header));
	if (std::memcmp(f.hdr.magic, "UBBS", 4) != 0)
		return std::nullopt;
	if (f.hdr.header_size != 0x10)
		return std::nullopt;

	std::memcpy(f.prefix.data(), data.data() + prefix_offset, f.prefix.size());

	const size_t name_count = size_t{f.hdr.mbs_count} + f.hdr.ftx_count;
	const size_t body_start = names_offset + name_count * 32;
	if (body_start > data.size())
		return std::nullopt;

	const uint64_t body_size =
		uint64_t{f.hdr.color_key_count} * sizeof(color_key) +
		uint64_t{f.hdr.section_72_count} * sizeof(record72) +
		uint64_t{f.hdr.section_108_count} * sizeof(record108) +
		uint64_t{f.hdr.section_68_count} * sizeof(record68) +
		uint64_t{f.hdr.section_24_count} * sizeof(record24) +
		uint64_t{f.hdr.section_128_count} * sizeof(record128) +
		uint64_t{f.hdr.section_16_count} * sizeof(record16);
	if (body_size != data.size() - body_start)
		return std::nullopt;

	size_t name_offset = names_offset;
	for (size_t i = 0; i < f.hdr.mbs_count; ++i) {
		const auto* text = data.data() + name_offset;
		size_t len = 0;
		while (len < 32 && text[len] != '\0')
			++len;
		f.mbs_names.emplace_back(reinterpret_cast<const char*>(text), len);
		name_offset += 32;
	}
	for (size_t i = 0; i < f.hdr.ftx_count; ++i) {
		const auto* text = data.data() + name_offset;
		size_t len = 0;
		while (len < 32 && text[len] != '\0')
			++len;
		f.ftx_names.emplace_back(reinterpret_cast<const char*>(text), len);
		name_offset += 32;
	}

	size_t offset = body_start;
	read_records(data, offset, f.hdr.color_key_count, f.color_keys);
	read_records(data, offset, f.hdr.section_72_count, f.section_72);
	read_records(data, offset, f.hdr.section_108_count, f.section_108);
	read_records(data, offset, f.hdr.section_68_count, f.section_68);
	read_records(data, offset, f.hdr.section_24_count, f.section_24);
	read_records(data, offset, f.hdr.section_128_count, f.section_128);
	read_records(data, offset, f.hdr.section_16_count, f.section_16);

	if (offset != data.size())
		return std::nullopt;

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

} // namespace bbs
