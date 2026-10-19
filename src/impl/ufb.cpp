#include "ufb.hpp"

#include <cstring>

namespace ufb {

namespace {

bool valid_utf8(std::span<const uint8_t> text) {
	size_t i = 0;
	while (i < text.size()) {
		const uint8_t c = text[i++];
		if (c < 0x80) {
			if (c < 0x20 || c == 0x7f)
				return false;
			continue;
		}

		size_t extra = 0;
		if (c >= 0xc2 && c <= 0xdf)
			extra = 1;
		else if (c >= 0xe0 && c <= 0xef)
			extra = 2;
		else if (c >= 0xf0 && c <= 0xf4)
			extra = 3;
		else
			return false;

		if (text.size() - i < extra)
			return false;
		for (size_t j = 0; j < extra; ++j) {
			if ((text[i] & 0xc0) != 0x80)
				return false;
			++i;
		}
	}
	return true;
}

size_t count_strings(std::span<const uint8_t> data) {
	size_t count = 0;
	size_t pos = 0;
	while (pos < data.size()) {
		if (data[pos] == '\0') {
			++pos;
			continue;
		}

		size_t end = pos;
		while (end < data.size() && data[end] != '\0')
			++end;
		if (!valid_utf8(data.subspan(pos, end - pos)))
			return 0;
		++count;
		pos = end;
	}
	return count;
}

std::optional<size_t> find_string_pool(std::span<const uint8_t> data) {
	if (data.empty())
		return std::nullopt;

	size_t suffix = data.size();
	while (suffix > 0) {
		const uint8_t c = data[suffix - 1];
		if (c != 0 && (c < 0x20 || c == 0x7f))
			break;
		--suffix;
	}

	for (size_t start = suffix; start < data.size(); ++start) {
		const uint8_t c = data[start];
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c >= 0xc2))
			continue;
		if (count_strings(data.subspan(start)) >= 8)
			return start;
	}
	return std::nullopt;
}

} // namespace

std::optional<file> parse(std::span<const uint8_t> data) {
	if (data.size() < sizeof(header) + 0x30)
		return std::nullopt;

	file f;
	std::memcpy(&f.hdr, data.data(), sizeof(header));
	if (std::memcmp(f.hdr.magic, "UCFB", 4) != 0)
		return std::nullopt;
	if (f.hdr.width == 0 || f.hdr.height == 0 || f.hdr.cell_size == 0)
		return std::nullopt;
	if (f.hdr.cell_count != size_t{f.hdr.width} * f.hdr.height)
		return std::nullopt;
	if (f.hdr.size_x != f.hdr.max_x - f.hdr.min_x || f.hdr.size_y != f.hdr.max_y - f.hdr.min_y)
		return std::nullopt;

	std::memcpy(f.prefix.data(), data.data() + sizeof(header), f.prefix.size());

	const auto strings_offset = find_string_pool(data);
	if (!strings_offset || *strings_offset < body_offset)
		return std::nullopt;
	f.strings_offset = *strings_offset;

	const size_t body_bytes = f.strings_offset - body_offset;
	if (body_bytes % sizeof(record) != 0)
		return std::nullopt;

	const size_t record_count = body_bytes / sizeof(record);
	if (record_count > data.size() / sizeof(record))
		return std::nullopt;

	f.records.resize(record_count);
	if (body_bytes != 0)
		std::memcpy(f.records.data(), data.data() + body_offset, body_bytes);

	size_t pos = f.strings_offset;
	while (pos < data.size()) {
		if (data[pos] == '\0') {
			++pos;
			continue;
		}
		size_t end = pos;
		while (end < data.size() && data[end] != '\0')
			++end;
		const auto text = data.subspan(pos, end - pos);
		if (!valid_utf8(text))
			return std::nullopt;
		f.strings.emplace_back(reinterpret_cast<const char*>(text.data()), text.size());
		pos = end;
	}

	if (f.hdr.asset_count > f.strings.size() / 2)
		return std::nullopt;
	const size_t asset_strings = size_t{f.hdr.asset_count} * 2;
	const size_t named_strings =
		size_t{f.hdr.object_name_count} + f.hdr.tile_name_count + f.hdr.region_name_count;
	if (asset_strings + named_strings != f.strings.size())
		return std::nullopt;

	f.assets.reserve(f.hdr.asset_count);
	for (size_t i = 0; i < f.hdr.asset_count; ++i)
		f.assets.push_back({f.strings[i * 2], f.strings[i * 2 + 1]});

	const auto names = std::span<const std::string>(f.strings).subspan(asset_strings);
	const auto object_end = f.hdr.object_name_count;
	const auto tile_end = object_end + f.hdr.tile_name_count;
	f.object_names.assign(names.begin(), names.begin() + object_end);
	f.tile_names.assign(names.begin() + object_end, names.begin() + tile_end);
	f.region_names.assign(names.begin() + tile_end, names.end());

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

} // namespace ufb
