#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <span>

struct byte_reader {
	const uint8_t* data;
	const uint8_t* cur;
	size_t size;

	explicit constexpr byte_reader(std::span<const uint8_t> s) noexcept
		: data(s.data())
		, cur(s.data())
		, size(s.size()) {}

	template <typename T>
	T read() noexcept {
		T val;
		assert(cur + sizeof(T) <= data + size);
		std::memcpy(&val, cur, sizeof(T));
		cur += sizeof(T);
		return val;
	}

	constexpr std::span<const uint8_t> read_bytes(size_t n) noexcept {
		assert(cur + n <= data + size);
		auto s = std::span<const uint8_t>(cur, n);
		cur += n;
		return s;
	}

	constexpr void seek(size_t offset) noexcept {
		assert(offset <= size);
		cur = data + offset;
	}

	constexpr void skip(ptrdiff_t n) noexcept {
		cur += n;
		assert(cur >= data && cur <= data + size);
	}

	constexpr size_t tell() const noexcept { return static_cast<size_t>(cur - data); }

	constexpr bool eof() const noexcept { return cur >= data + size; }

	constexpr explicit operator bool() const noexcept { return !eof(); }
};
