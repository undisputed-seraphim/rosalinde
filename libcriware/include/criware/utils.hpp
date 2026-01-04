#pragma once

#include <concepts>
#include <istream>
#include <string>

#include <criware/endian_swap.hpp>

template <typename T>
concept Fundamental = std::is_fundamental_v<T>;

template <typename T>
concept Numeric = (std::integral<T> || std::floating_point<T>);

template <typename T>
concept Trivial = std::is_trivial_v<T>;

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

template <Trivial T>
[[nodiscard]] T read_value(std::istream& i) {
	T value;
	i.read((char*)&value, sizeof(value));
	return value;
}

template <Fundamental T>
[[nodiscard]] T read_value_swap_endian(std::istream& i) {
	return swap_endian(read_value<T>(i));
}
