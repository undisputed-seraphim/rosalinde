#include "mbs.hpp"
#include "mbs/sections.hpp"
#include "utils.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

// Faulty struct, do not use
struct v77_1 {
	uint32_t longvals[5];
	uint16_t shortvals[7];
	uint64_t _unknown1[2];
	char name[32];
	uint64_t _unknown2[2];
	uint64_t offsets[12];
};

#pragma pack(push, 1)
struct mbs_header {
	char magic[4];
	uint32_t file_size;
	uint32_t header_size;
	uint32_t _pad0;
	uint16_t _unk0;
	uint16_t _unk1;
	uint16_t version;
	uint16_t _unk2;
	uint32_t _unk3;
	uint32_t _pad1;

	static constexpr std::string_view FMBS = "FMBS";
};
#pragma pack(pop)

MBS::MBS(std::istream& is)
	: MBS(std::move(is)) {}

MBS::MBS(std::istream&& is)
	: _filename(2 * 16, char(0))
	, data(read(is)) {}

MBS::MBS(MBS&&) noexcept = default;

MBS::~MBS() noexcept {}

const mbs::v77& MBS::get() const {
	return data;
}

mbs::v77 MBS::read(std::istream& is) {
	is.seekg(0, std::ios::beg);
	const mbs_header h = read_value<mbs_header>(is);
	if (std::string_view(h.magic, sizeof(h.magic)) != mbs_header::FMBS) {
		throw std::exception("Not an FMBS file.");
	}

	is.seekg(0x80, std::ios::beg);
	is.read(_filename.data(), _filename.size());
	trim_string(_filename);

	switch (h.version) {
	case 0x77: {
		return mbs::v77::read(is);
	}
	case 0x66:
	case 0x6b:
	case 0x6d:
	case 0x6e:
	case 0x72:
	case 0x76:
	default: {
		std::cout << "Unsupported FMBS version " << (uint16_t)h.version << std::endl;
	}
	}
	throw std::exception("Unsupported FMBS version.");
}
