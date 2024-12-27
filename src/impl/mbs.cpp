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

struct MBS::data_t : public std::variant<mbs::v77, std::monostate> {
	~data_t() noexcept {}
};

MBS::MBS(std::istream& is)
	: MBS(std::move(is)) {}

MBS::MBS(std::istream&& is)
	: _filename(2 * 16, char(0))
	, _dataptr(read(is)) {}

MBS::MBS(MBS&&) noexcept = default;

MBS::~MBS() noexcept { _dataptr.reset(); }

Quad MBS::extract() const {
	auto makequad_v77 = [](const mbs::v77& v77) -> Quad { v77.print_to_file(); return v77.to_quad(); };
	auto monostate = [](const std::monostate&) -> Quad { return Quad{}; };
	return std::visit(overloads{makequad_v77, monostate}, *_dataptr);
}

std::unique_ptr<MBS::data_t> MBS::read(std::istream& is) {
	is.seekg(0, std::ios::beg);
	const mbs_header h = read_value<mbs_header>(is);
	if (std::string_view(h.magic, sizeof(h.magic)) != mbs_header::FMBS) {
		std::cout << "Not an FMBS file." << std::endl;
		return std::make_unique<MBS::data_t>(std::monostate{});
	}

	is.seekg(0x80, std::ios::beg);
	is.read(_filename.data(), _filename.size());
	trim_string(_filename);

	switch (h.version) {
	case 0x77: {
		return std::make_unique<data_t>(mbs::v77::read(is));
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
	return std::make_unique<MBS::data_t>(std::monostate{});
}
