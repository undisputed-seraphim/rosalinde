#include "fms.hpp"

#include <criware/byte_reader.hpp>

// FMSB container (Criware ".fms" message/string tables). Layout, all little
// endian:
//   0x00  magic "FMSB"
//   0x04  data_size   = file size - 48
//   0x08  header_size = 0x20
//   0x0c  unknown1
//   0x10  unknown2
//   0x14  string_count
//   0x18  unknown3 (always 3)
//   0x1c  unknown4
//   0x20  string_count * { uint32 prop1, uint32 prop2 }  (always zero)
//   ...   string_count NUL-terminated UTF-8 strings, stored end to end
//   ...   zero padding, up to a 16-byte alignment
//   0x??  footer "FEOC", unknown5, footer_size(0x10), unknown6

namespace fms {

namespace {

constexpr uint32_t magic_fmsb = 0x42534d46;
constexpr uint32_t magic_feoc = 0x434f4546;
constexpr uint32_t expected_header_size = 0x20;
constexpr uint32_t expected_footer_size = 0x10;

}

std::optional<table> parse_fms(std::span<const uint8_t> data) {
	if (data.size() < 0x20)
		return std::nullopt;

	byte_reader r{data};
	if (r.read<uint32_t>() != magic_fmsb)
		return std::nullopt;

	table t;
	t.data_size = r.read<uint32_t>();
	t.header_size = r.read<uint32_t>();
	t.unknown1 = r.read<uint32_t>();
	t.unknown2 = r.read<uint32_t>();
	const uint32_t count = r.read<uint32_t>();
	t.unknown3 = r.read<uint32_t>();
	t.unknown4 = r.read<uint32_t>();

	if (t.header_size != expected_header_size)
		return std::nullopt;

	if (count > (r.size - r.tell()) / 8)
		return std::nullopt;

	t.strings.resize(count);
	for (auto& s : t.strings) {
		s.prop1 = r.read<uint32_t>();
		s.prop2 = r.read<uint32_t>();
	}

	for (auto& s : t.strings) {
		const uint8_t* begin = r.cur;
		while (!r.eof() && *r.cur != 0)
			++r.cur;
		if (r.eof())
			return std::nullopt;
		s.text.assign(reinterpret_cast<const char*>(begin), reinterpret_cast<const char*>(r.cur));
		r.skip(1);
	}

	const size_t overage = r.tell() % 16;
	if (overage != 0) {
		const size_t padding = 16 - overage;
		if (r.size - r.tell() < padding)
			return std::nullopt;
		r.skip(static_cast<ptrdiff_t>(padding));
	}

	if (r.size - r.tell() < 16)
		return std::nullopt;
	if (r.read<uint32_t>() != magic_feoc)
		return std::nullopt;

	t.unknown5 = r.read<uint32_t>();
	t.footer_size = r.read<uint32_t>();
	t.unknown6 = r.read<uint32_t>();

	if (t.footer_size != expected_footer_size)
		return std::nullopt;

	return t;
}

std::optional<table> parse_fms(std::istream& is) {
	std::vector<uint8_t> buffer;
	char c;
	while (is.get(c))
		buffer.push_back(static_cast<uint8_t>(c));
	return parse_fms(std::span<const uint8_t>(buffer.data(), buffer.size()));
}

}
