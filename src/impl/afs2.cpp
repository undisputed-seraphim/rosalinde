#include "afs2.hpp"
#include "../utils.hpp"
#include "utf.hpp"

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

namespace AFS2 {

std::vector<Entry> parse(std::istream&& is) {
	is.seekg(0, std::ios::end);
	const uint64_t totalsize = is.tellg();
	is.seekg(0, std::ios::beg);

	const auto hdr = read_value<header>(is);
	if (::strncmp(hdr.afs2, MAGIC, sizeof(hdr.afs2)) != 0) {
		return {};
	}
	if (hdr.numfiles == 0) {
		return {};
	}

	std::vector<Entry> entries(hdr.numfiles);
	if (hdr.id_size == 4) {
		for (int i = 0; i < hdr.numfiles; ++i) {
			entries[i].id = read_value<uint32_t>(is);
		}
	}
	if (hdr.offset_size == 4) {
		for (int i = 0; i < hdr.numfiles; ++i) {
			entries[i].offset = read_value<uint32_t>(is);
			// TODO: We may potentially have to add another header offset.
			// entries[i].offset += sizeof(header);
		}
	}
	for (int i = 0; i < hdr.numfiles; ++i) {
		if (i == hdr.numfiles - 1) {
			entries[i].size = totalsize - entries[i].offset;
		} else {
			entries[i].size = entries[i + 1].offset - entries[i].offset;
		}
	}
	return entries;
}

bool extract(const Entry& e, std::istream& is, std::vector<char>& buffer) {
	buffer.resize(e.size);
	return bool(is.seekg(e.offset, std::ios::beg).read(buffer.data(), e.size));
}

void generate_table() {
	static const std::vector<std::string> tables = {
		"CueId",
		"ReferenceIndex",
		"AisacControlMap",
		"Length",
		"NumAisacControlMaps",
		"NumRelatedWaveforms",
		"CueName",
		"CueIndex",
		"NumTracks",
		"TrackIndex",
		"CommandIndex",
		"LocalAisacs",
		"GlobalAisacStartIndex",
		"GlobalAisacNumRefs",
		"ParameterPallet",
		"ActionTrackStartIndex",
		"NumActionTracks",
		"Type",
		"ControlWorkArea1"};
}

void parse(const UTF& utf_table) {
	for (const auto& entry : utf_table) {
		if (!entry.name.ends_with("Table")) {
			continue;
		}
		if (entry.type_ != UTF::field::type::DATA_ARRAY) {
			continue;
		}
		auto data = entry.cast_at<std::vector<char>>(0).value_or(std::vector<char>{});
		const UTF subtable(data);
		if (!subtable.empty()) {
			std::cout << "-----> Reading sub table " << entry.name << " with " << entry.values.size() << " entries:\n";
			UTF::dump(subtable);
			std::cout << std::endl;
		}
	}
}

AFS2::AFS2(std::istream&& acb, std::istream&& awb)
	: _awb_entries(parse(std::move(awb))) {
	const auto table = UTF(acb);
	parse(table);
}

} // namespace AFS2
