#include "cpk.hpp"
#include "endian_swap.hpp"
#include "streams.hpp"
#include "utf.hpp"
#include "utils.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <span>
#include <stdexcept>

constexpr uint32_t CPK_magic = 0x204B5043; // "CPK " reversed endian
constexpr uint32_t TOC_magic = 0x20434F54; // "TOC " reversed endian

// crilayla.cpp
int64_t compress(const std::vector<char>& src, std::vector<char>& dst);
int64_t decompress(std::span<char> in, std::vector<char>& out);

static uint64_t read_header(std::istream& i, const uint32_t magic) {
#pragma pack(push, 1)
	struct table_header {
		uint32_t magic;
		uint32_t _pad;
		uint64_t length;
	};
#pragma pack(pop)
	table_header header;
	i.read((char*)(&header), sizeof(header));
	if (header.magic != magic) {
		throw std::runtime_error(
			"Mismatched magic bytes at offset " + std::to_string(size_t(i.tellg()) - sizeof(header)));
	}
	return header.length;
}

//====

TopLevelCpk::TopLevelCpk(std::filesystem::path path)
	: UTFTable<TopLevelCPKTraits>()
	, _path(std::move(path)) {
	auto ifs = std::ifstream(_path, std::ios::binary);
	const uint64_t size = read_header(ifs, CPK_magic);
	_buffer.resize(size);
	ifs.read(_buffer.data(), size);
	UTF::decipher(_buffer);
	auto iss = std::ispanstream(_buffer);
	Base::operator>>(iss);
}

CPKTable TopLevelCpk::getTableOfContents() const {
	const auto& [TocOffset] = this->at(0);
	auto ifs = std::ifstream(_path, std::ios::binary);
	ifs.seekg(TocOffset, std::ios::beg);
	const uint64_t size = read_header(ifs, TOC_magic);
	_buffer.resize(size);
	ifs.read(_buffer.data(), size);
	UTF::decipher(_buffer);
	auto table = CPKTable(_path, TocOffset);
	std::ispanstream(_buffer) >> table;
	return table;
}

//====

bool CPKTraits::IsValidEntry(const Entry& e) noexcept { return (std::get<2>(e) > 0) && (std::get<3>(e) > 0); }

CPKTable::CPKTable(std::filesystem::path path, uint64_t offset)
	: UTFTable<CPKTraits>()
	, _path(std::move(path))
	, _offset(offset) {}

void CPKTable::extract(const entry_tuple& entry, std::vector<char>& out) const {
	this->extract(std::get<0>(entry), std::get<1>(entry), out);
}

void CPKTable::extract(std::string_view dir, std::string_view file, std::vector<char>& out) const {
	auto it = find_file(dir, file);
	if (it == this->end()) {
		return;
	}
	const auto& [DirName, FileName, FileSize, ExtractSize, FileOffset, ID] = *it;
	_buffer.resize(FileSize);
	std::ifstream(_path, std::ios::binary).seekg(FileOffset + _offset, std::ios::beg).read(_buffer.data(), FileSize);
	if (ExtractSize == FileSize) {
		std::swap(_buffer, out);
		return;
	}
	out.resize(ExtractSize);
	[[maybe_unused]] const auto ret = decompress(_buffer, out);
}

CPKTable::iterator CPKTable::find_file(std::string_view dir, std::string_view file) const {
	iterator it = this->begin();
	for (; it != this->end(); ++it) {
		const auto& dirname = std::get<0>(*it);
		const auto& filename = std::get<1>(*it);
		if (dir == dirname && file == filename) {
			break;
		}
	}
	return it;
}