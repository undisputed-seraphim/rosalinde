#pragma once

#include "../utils.hpp"
#include <iostream>
#include <string_view>
#include <vector>

// Looks like a bunch of integers with some floats mixed in every nth value.
inline std::vector<uint32_t> parse_nnd(std::istream& is) {
#pragma pack(push, 1)
	struct NNB {
		uint32_t ver;
		uint32_t num;
	};
#pragma pack(pop)

	const auto header = read_value<NNB>(is);
	if (header.ver != 2) {
		return {};
	}
	std::vector<uint32_t> out(header.num);
	is.read((char*)out.data(), sizeof(uint32_t) * header.num);
	return out;
}
