#pragma once

#include "sections.hpp"
#include "../byte_reader.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace mbs::detail {

struct lookup_entry {
	int16_t c;          // file offset for element count
	int16_t c_size;     // byte size of count field (2 or 4)
	int16_t p;          // file offset for data-start pointer
	int16_t row_length; // byte size of a single row
	uint32_t h = 0;     // resolved: number of rows
	uint32_t offset = 0; // resolved: file offset to data start
	uint32_t data_size = 0; // resolved: h * row_length
};

template <typename T>
std::vector<T> read_block(byte_reader& r, const lookup_entry& e) {
	if (e.c == 0 || e.p == 0 || e.h == 0) {
		return {};
	}
	r.seek(e.offset);
	auto bytes = r.read_bytes(e.data_size);
	std::vector<T> entries(e.h);
	std::memcpy(entries.data(), bytes.data(), e.data_size);
	return entries;
}

inline void resolve_table(byte_reader& r, std::span<lookup_entry> table) {
	for (auto& [c, cs, p, rl, h, o, ds] : table) {
		if (c == 0 || p == 0) {
			continue;
		}
		r.seek(c);
		switch (cs) {
		case 2: h = r.read<uint16_t>(); break;
		case 4: h = r.read<uint32_t>(); break;
		}
		r.seek(p);
		o = r.read<uint32_t>();
		ds = h * rl;
	}
}

inline void populate_sections(byte_reader& r, v77& v, std::span<const lookup_entry> table) {
	// Section index corresponds to table position; order must match
	// the packed enum in v77 (s0=0, s1=1, ... sb=11).
	v.s0 = read_block<section_0>(r, table[0x0]);
	v.s1 = read_block<section_1>(r, table[0x1]);
	v.s2 = read_block<section_2>(r, table[0x2]);
	v.s3 = read_block<section_3>(r, table[0x3]);
	v.s4 = read_block<section_4>(r, table[0x4]);
	v.s5 = read_block<section_5>(r, table[0x5]);
	v.s6 = read_block<section_6>(r, table[0x6]);
	v.s7 = read_block<section_7>(r, table[0x7]);
	v.s8 = read_block<section_8>(r, table[0x8]);
	v.s9 = read_block<section_9>(r, table[0x9]);
	v.sa = read_block<section_a>(r, table[0xa]);
	v.sb = read_block<section_b>(r, table[0xb]);
}

} // namespace mbs::detail

namespace mbs {

void parse_v76(std::span<const uint8_t>, v77&);
void parse_v77(std::span<const uint8_t>, v77&);

} // namespace mbs
