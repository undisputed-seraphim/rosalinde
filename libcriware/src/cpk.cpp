#include <criware/cpk.hpp>

#include <cstring>
#include <fstream>
#include <istream>
#include <limits>
#include <stdexcept>

// crilayla.cpp
int64_t compress(const std::vector<char>& src, std::vector<char>& dst);
int64_t decompress(std::span<const char> in, std::span<char> out);
int64_t decompress(std::istream& in, uint64_t offset, uint64_t file_size, std::span<char> out);

constexpr uint32_t CPK_magic = 0x204B5043;
constexpr uint32_t TOC_magic = 0x20434F54;

static uint64_t read_header(std::istream& i, uint32_t magic) {
#pragma pack(push, 1)
	struct {
		uint32_t magic;
		uint32_t _pad;
		uint64_t length;
	} hdr;
#pragma pack(pop)
	i.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
	if (hdr.magic != magic)
		throw std::runtime_error(
			"Mismatched magic bytes at offset " + std::to_string(static_cast<size_t>(i.tellg()) - sizeof(hdr)));
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
	_table =
		table<TopLevelCPKSchema>::parse(std::span(reinterpret_cast<const uint8_t*>(_buffer.data()), _buffer.size()));
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
	table.parse_toc(std::span(reinterpret_cast<const uint8_t*>(_buffer.data()), _buffer.size()));
	return table;
}

// ============================================================================
// CPKTable (uses new typed table<>)
// ============================================================================

CPKTable::CPKTable(std::filesystem::path path, uint64_t offset)
	: _path(std::move(path))
	, _offset(offset) {}

void CPKTable::parse_toc(std::span<const uint8_t> data) { _table = table_type::parse(data); }

void CPKTable::extract(const row_type& entry, std::vector<char>& out) const {
	out.resize(static_cast<size_t>(get<"ExtractSize">(entry)));
	extract(entry, std::span<char>(out));
}

void CPKTable::extract(const row_type& entry, std::span<char> out) const {
	const uint64_t file_size = get<"FileSize">(entry);
	const uint64_t extract_size = get<"ExtractSize">(entry);
	const uint64_t file_offset = get<"FileOffset">(entry);
	if (file_size > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
		extract_size > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
		file_size > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()) ||
		extract_size > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max())) {
		throw std::runtime_error("CPK entry is too large");
	}
	if (out.size() < extract_size) {
		throw std::runtime_error("CPK output buffer is too small");
	}
	out = out.first(static_cast<size_t>(extract_size));

	std::ifstream ifs(_path, std::ios::binary);
	if (!ifs) {
		throw std::runtime_error("Failed to open " + _path.string());
	}
	ifs.seekg(static_cast<std::streamoff>(file_offset + _offset), std::ios::beg);
	if (!ifs) {
		throw std::runtime_error("Failed to seek in " + _path.string());
	}

	if (extract_size == file_size) {
		ifs.read(out.data(), static_cast<std::streamsize>(extract_size));
		if (!ifs) {
			throw std::runtime_error("Failed to read " + _path.string());
		}
		return;
	}

	const auto ret = decompress(ifs, file_offset + _offset, file_size, out);
	if (ret < 0) {
		throw std::runtime_error("Failed to decompress " + _path.string());
	}
}

void CPKTable::extract(std::string_view dir, std::string_view file, std::vector<char>& out) const {
	auto it = find_file(dir, file);
	if (it == end())
		return;
	extract(*it, out);
}

void CPKTable::extract(std::string_view dir, std::string_view file, std::span<char> out) const {
	auto it = find_file(dir, file);
	if (it == end())
		return;
	extract(*it, out);
}

CPKTable::iterator CPKTable::find_file(std::string_view dir, std::string_view file) const {
	auto it = begin();
	for (; it != end(); ++it) {
		if (dir == get<"DirName">(*it) && file == get<"FileName">(*it))
			break;
	}
	return it;
}
