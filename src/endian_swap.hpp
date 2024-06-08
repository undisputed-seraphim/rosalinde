#pragma once

#include <climits>
#include <stdexcept>

namespace swap_endian_detail {

template <typename T, unsigned int B>
struct swap_endian_impl {
	inline T operator () (T t) {
		throw std::out_of_range("Unsupported endian swapping size.");
	}
};

template <typename T>
struct swap_endian_impl<T, 1> {
	inline T operator () (T t) {
		return t;
	}
};


template <typename T>
struct swap_endian_impl<T, 2> {
	inline T operator () (T t) {
		uint16_t u16 = *reinterpret_cast<uint16_t*>(&t);
#ifdef _MSC_VER
		u16 = _byteswap_ushort(u16);
#else
		u16 = __builtin_bswap16(u16);
#endif
		return *reinterpret_cast<T*>(&u16);
	}
};


template <typename T>
struct swap_endian_impl<T, 4> {
	inline T operator () (T t) {
		uint32_t u32 = *reinterpret_cast<uint32_t*>(&t);
#ifdef _MSC_VER
		u32 = _byteswap_ulong(u32);
#else
		u32 = __builtin_bswap32(u32);
#endif
		return *reinterpret_cast<T*>(&u32);
	}
};


template <typename T>
struct swap_endian_impl<T, 8> {
	inline T operator () (T t) {
		uint64_t u64 = *reinterpret_cast<uint64_t*>(&t);
#ifdef _MSC_VER
		u64 = _byteswap_uint64(u64);
#else
		u64 = __builtin_bswap64(u64);
#endif
		return *reinterpret_cast<T*>(&u64);
	}
};

} // namespace swap_endian_detail

// Call this function.
template <typename T>
inline T swap_endian(T t) {
	static_assert(CHAR_BIT == 8, "Byte is not 8 bits on this platform.");
	static_assert(std::is_fundamental_v<T>, "Type is not a primitive.");
	return swap_endian_detail::swap_endian_impl<T, sizeof(T)>()(t);
}
