#include "mbs.hpp"
#include "byte_reader.hpp"
#include "mbs/detail.hpp"

#include <criware/utils.hpp>

#include <cstring>
#include <iostream>
#include <spanstream>
#include <stdexcept>

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

MBS::MBS()
	: _filename(32, '\0') {}

MBS::MBS(MBS&&) noexcept = default;

MBS::~MBS() noexcept {}

// --- istream path (drains to vector, delegates to span) -------------------

static std::vector<char> drain_stream(std::istream& is) {
	is.seekg(0, std::ios::end);
	const auto sz = static_cast<size_t>(is.tellg());
	is.seekg(0, std::ios::beg);
	std::vector<char> buf(sz);
	is.read(buf.data(), static_cast<std::streamsize>(sz));
	return buf;
}

MBS MBS::From(std::istream& is) { return MBS::From(drain_stream(is)); }

MBS MBS::From(const std::vector<char>& buf) {
	return MBS::From(std::span<const uint8_t>(
		reinterpret_cast<const uint8_t*>(buf.data()), buf.size()));
}

void MBS::parse(std::istream& is) {
	auto buf = drain_stream(is);
	parse(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buf.data()), buf.size()));
}

// --- span path (primary) --------------------------------------------------

MBS MBS::From(std::span<const uint8_t> data) {
	MBS mbs;
	mbs.parse(data);
	return mbs;
}

void MBS::parse(std::span<const uint8_t> data) {
	byte_reader r(data);

	const auto h = r.read<mbs_header>();
	if (std::strncmp(h.magic, mbs_header::FMBS.data(), sizeof(h.magic)) != 0) {
		throw std::runtime_error("Not an FMBS file.");
	}

	r.seek(0x80);
	auto name_bytes = r.read_bytes(_filename.size());
	_filename.assign(reinterpret_cast<const char*>(name_bytes.data()), _filename.size());
	trim_string(_filename);

	switch (h.version) {
	case 0x76: {
		_version = mbs::Version::v76;
		mbs::parse_v76(data, this->data);
		return;
	}
	case 0x77: {
		_version = mbs::Version::v77;
		mbs::parse_v77(data, this->data);
		return;
	}
	case 0x66:
	case 0x6b:
	case 0x6d:
	case 0x6e:
	case 0x72:
	default: {
		std::cout << "Unsupported FMBS version " << (uint16_t)h.version << std::endl;
	}
	}
	throw std::runtime_error("Unsupported FMBS version.");
}

const mbs::v77& MBS::get() const { return data; }
