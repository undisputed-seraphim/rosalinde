#pragma once

#include <cstdint>
#include <istream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace fms {

struct string_entry {
	uint32_t prop1 = 0;
	uint32_t prop2 = 0;
	std::string text;
};

struct table {
	uint32_t data_size = 0;
	uint32_t header_size = 0;
	uint32_t unknown1 = 0;
	uint32_t unknown2 = 0;
	uint32_t unknown3 = 0;
	uint32_t unknown4 = 0;
	uint32_t footer_size = 0;
	uint32_t unknown5 = 0;
	uint32_t unknown6 = 0;
	std::vector<string_entry> strings;

	const std::string& at(size_t index) const {
		static const std::string empty;
		return index < strings.size() ? strings[index].text : empty;
	}
};

std::optional<table> parse_fms(std::span<const uint8_t> data);
std::optional<table> parse_fms(std::istream& is);

}
