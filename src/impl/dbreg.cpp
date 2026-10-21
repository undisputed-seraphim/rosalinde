#include "dbreg.hpp"

#include <cstring>

namespace dbreg {

namespace {

template <size_t N>
std::string_view fixed_text(const char (&text)[N]) {
	size_t len = 0;
	while (len < N && text[len] != '\0')
		++len;
	return {text, len};
}

} // namespace

std::string_view source_entry::path_view() const { return fixed_text(path); }
std::string_view source_entry::tag_view() const { return fixed_text(tag); }
std::string_view key_entry::key_view() const { return fixed_text(key); }
std::string_view key_entry::description_view() const { return fixed_text(description); }

std::optional<file> parse(std::span<const uint8_t> data) {
	if (data.size() < sizeof(header))
		return std::nullopt;
	if (std::memcmp(data.data(), "DRG\0", 4) != 0)
		return std::nullopt;

	file f;
	std::memcpy(&f.hdr, data.data(), sizeof(header));

	const uint64_t key_end =
		uint64_t{f.hdr.key_offset} + uint64_t{f.hdr.key_count} * sizeof(key_entry);
	if (f.hdr.key_offset >= sizeof(header) && key_end == data.size()) {
		if (f.hdr.source_count > (data.size() - sizeof(header)) / sizeof(source_entry))
			return std::nullopt;
		f.type = kind::catalog;
		f.sources.resize(f.hdr.source_count);
		if (!f.sources.empty())
			std::memcpy(f.sources.data(), data.data() + sizeof(header), f.sources.size() * sizeof(source_entry));
		f.keys.resize(f.hdr.key_count);
		if (!f.keys.empty())
			std::memcpy(f.keys.data(), data.data() + f.hdr.key_offset, f.keys.size() * sizeof(key_entry));
		return f;
	}

	if ((data.size() - 4) % sizeof(uint32_t) != 0)
		return std::nullopt;
	f.type = kind::values;
	f.values.resize((data.size() - 4) / sizeof(uint32_t));
	if (!f.values.empty())
		std::memcpy(f.values.data(), data.data() + 4, f.values.size() * sizeof(uint32_t));
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

} // namespace dbreg
