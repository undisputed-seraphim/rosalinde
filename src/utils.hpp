#pragma once

#include <concepts>
#include <istream>
#include <string>

#include "endian_swap.hpp"

template <typename T>
concept Fundamental = std::is_fundamental_v<T>;

template <typename T>
concept Numeric = (std::integral<T> || std::floating_point<T>);

template <typename T>
concept Trivial = std::is_trivial_v<T>;

template <class... Ts>
struct overloads : Ts... {
	using Ts::operator()...;
};

template <Numeric T, Numeric U>
bool are_equal(T t, U u) {
	return t == u;
}

template <Numeric T, Numeric U, Numeric... Others>
bool are_equal(T t, U u, Others... args) {
	return (t == u) && are_equal(u, args...);
}

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

template <unsigned int Num>
std::istream& discard_bytes(std::istream& i) {
	return i.seekg(Num, std::ios::cur);
}

template <typename T>
struct hex {
	const T& _val;

	std::ostream& operator<<(std::ostream& o) const {
		o << std::hex << _val << std::dec;
		return o;
	}

	template <typename U>
	friend std::ostream& operator<<(std::ostream&, const hex<U>&);
};

template <typename T>
std::ostream& operator<<(std::ostream& o, const hex<T>& h) {
	return h.operator<<(o);
}
