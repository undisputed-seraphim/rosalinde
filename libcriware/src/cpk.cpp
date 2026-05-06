#include <criware/cpk.hpp>

#include <cstring>
#include <fstream>
#include <spanstream>
#include <stdexcept>

// crilayla.cpp
int64_t compress(const std::vector<char>& src, std::vector<char>& dst);
int64_t decompress(std::span<char> in, std::vector<char>& out);

constexpr uint32_t CPK_magic = 0x204B5043;
constexpr uint32_t TOC_magic = 0x20434F54;

static uint64_t read_header(std::istream& i, uint32_t magic) {
#pragma pack(push, 1)
	struct { uint32_t magic; uint32_t _pad; uint64_t length; } hdr;
#pragma pack(pop)
	i.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
	if (hdr.magic != magic)
		throw std::runtime_error("Mismatched magic bytes at offset "
			+ std::to_string(static_cast<size_t>(i.tellg()) - sizeof(hdr)));
	return hdr.length;
}

// ============================================================================
// TopLevelCpk
// ============================================================================

TopLevelCpk::TopLevelCpk(std::filesystem::path path)
	: _path(std::move(path)) {
	auto ifs = std::ifstream(_path, std::ios::binary);
	const uint64_t size = read_header(ifs, CPK_magic);
	_buffer.resize(size);
	ifs.read(_buffer.data(), static_cast<std::streamsize>(size));
	UTF::decipher(_buffer);
	auto iss = std::ispanstream(_buffer);
	iss >> _table;
}

CPKTable TopLevelCpk::getTableOfContents() const {
	const auto [TocOffset] = _table[0];
	auto ifs = std::ifstream(_path, std::ios::binary);
	ifs.seekg(static_cast<std::streamoff>(TocOffset), std::ios::beg);
	const uint64_t size = read_header(ifs, TOC_magic);
	_buffer.resize(static_cast<size_t>(size));
	ifs.read(_buffer.data(), static_cast<std::streamsize>(size));
	UTF::decipher(_buffer);
	CPKTable table(_path, TocOffset);
	std::ispanstream(_buffer) >> table;
	return table;
}

// ============================================================================
// CPKTable (uses new typed table<>)
// ============================================================================

CPKTable::CPKTable(std::filesystem::path path, uint64_t offset)
	: _path(std::move(path))
	, _offset(offset) {}

void CPKTable::extract(const row_type& entry, std::vector<char>& out) const {
	const auto& [dir, file, fileSize, extractSize, fileOffset, id] = entry;
	extract(dir, file, out);
}

void CPKTable::extract(std::string_view dir, std::string_view file, std::vector<char>& out) const {
	auto it = find_file(dir, file);
	if (it == end()) return;
	const auto& [DirName, FileName, FileSize, ExtractSize, FileOffset, ID] = *it;

	_buffer.resize(static_cast<size_t>(FileSize));
	std::ifstream(_path, std::ios::binary)
		.seekg(static_cast<std::streamoff>(FileOffset + _offset), std::ios::beg)
		.read(_buffer.data(), static_cast<std::streamsize>(FileSize));
	if (ExtractSize == FileSize) {
		std::swap(_buffer, out);
		return;
	}
	out.resize(static_cast<size_t>(ExtractSize));
	[[maybe_unused]] const auto ret = decompress(_buffer, out);
}

CPKTable::iterator CPKTable::find_file(std::string_view dir, std::string_view file) const {
	auto it = begin();
	for (; it != end(); ++it) {
		const auto& [DirName, FileName, FileSize, ExtractSize, FileOffset, ID] = *it;
		if (dir == DirName && file == FileName)
			break;
	}
	return it;
}

