#pragma once

#include "../utils.hpp"
#include <iostream>
#include <string_view>
#include <vector>

inline std::vector<std::string> parse_fms(std::istream& is) {
#pragma pack(push, 1)
	struct FMS {
		char fmsb[4];
		uint32_t size;
		uint32_t _unk0;
		uint32_t _pad0;
		uint32_t _pad1;
		uint32_t rows;
		uint32_t sections_maybe;
	};
#pragma pack(pop)
	static constexpr std::string_view MAGIC = "FMSB";

	const auto header = read_value<FMS>(is);
	if (::strncmp(header.fmsb, MAGIC.data(), sizeof(header.fmsb)) != 0) {
		return {};
	}

	while (is.peek() == 0) {
		is.seekg(1, std::ios::cur);
	}

	std::vector<std::string> out(header.rows);
	for (uint32_t i = 0; i < header.rows; ++i) {
		std::getline(is, out[i], char(0));
	}
	return out;
}
