#pragma once

#include <concepts>
#include <istream>
#include <string>

#include <criware/endian_swap.hpp>

inline void trim_string(std::string& str) {
	while (!isalnum(str.back())) {
		str.pop_back();
	}
	str.shrink_to_fit();
}

inline void trim_string(std::string_view& str) {
	while (!isalnum(str.back())) {
		str.remove_suffix(1);
	}
}

template <typename T>
requires std::is_trivial_v<T>
[[nodiscard]] T read_value(std::istream& i) {
	T value;
	i.read((char*)&value, sizeof(value));
	return value;
}

template <typename T>
requires std::is_fundamental_v<T>
[[nodiscard]] T read_value_swap_endian(std::istream& i) {
	return swap_endian(read_value<T>(i));
}
