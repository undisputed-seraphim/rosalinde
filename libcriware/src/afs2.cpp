#include <criware/afs2.hpp>
#include <criware/cpk.hpp>
#include <criware/utils.hpp>

#include <cstring>
#include <iostream>
#include <span>
#include <spanstream>

namespace {

#pragma pack(push, 1)
struct header {
	char afs2[4];
	uint8_t version;
	uint8_t offset_size;
	uint16_t id_size;
	uint32_t numfiles;
	uint16_t alignment;
	uint16_t subkey;
};
#pragma pack(pop)

constexpr const char MAGIC[5] = "AFS2";

} // anonymous namespace

std::istream& operator>>(std::istream& is, AFS2& afs2) {
	is.seekg(0, std::ios::end);
	const uint64_t totalsize = is.tellg();
	is.seekg(0, std::ios::beg);

	const auto hdr = read_value<header>(is);
	if (::strncmp(hdr.afs2, MAGIC, sizeof(hdr.afs2)) != 0) {
		return is;
	}
	if (hdr.numfiles == 0) {
		return is;
	}

	afs2._entries.resize(hdr.numfiles);

	switch (hdr.id_size) {
	case 2: {
		for (uint32_t i = 0; i < hdr.numfiles; ++i) {
			afs2._entries[i].id = read_value<uint16_t>(is);
		}
		break;
	}
	case 4: {
		for (uint32_t i = 0; i < hdr.numfiles; ++i) {
			afs2._entries[i].id = read_value<uint32_t>(is);
		}
		break;
	}
	default: {
		// Invalid
		return is;
	}
	}

	// TODO: We may potentially have to add another header offset.
	// entries[i].offset += sizeof(header);
	switch (hdr.offset_size) {
	case 2: {
		for (uint32_t i = 0; i < hdr.numfiles; ++i) {
			afs2._entries[i].offset = read_value<uint16_t>(is);
		}
		break;
	}
	case 4: {
		for (uint32_t i = 0; i < hdr.numfiles; ++i) {
			afs2._entries[i].offset = read_value<uint32_t>(is);
		}
		break;
	}
	default: {
		// Invalid
		return is;
	}
	}
	for (uint32_t i = 0; i < hdr.numfiles; ++i) {
		if (i == hdr.numfiles - 1) {
			afs2._entries[i].size = totalsize - afs2._entries[i].offset;
		} else {
			afs2._entries[i].size = afs2._entries[i + 1].offset - afs2._entries[i].offset;
		}
	}

	return is;
}
