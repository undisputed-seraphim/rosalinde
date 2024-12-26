#include "cpk.hpp"
#include "endian_swap.hpp"
#include "streams.hpp"
#include "utf.hpp"
#include "utils.hpp"

#include <algorithm>
#include <fstream>
#include <span>
#include <stdexcept>

constexpr uint32_t CPK_magic = 0x204B5043; // "CPK " reversed endian
constexpr uint32_t TOC_magic = 0x20434F54; // "TOC " reversed endian

// crilayla.cpp
int64_t compress(const std::vector<char>& src, std::vector<char>& dst);
int64_t decompress(std::span<char> in, std::vector<char>& out);

const std::filesystem::path& CPK::directory_entry::path() const noexcept { return _path; }
CPK::directory_entry::operator const std::filesystem::path&() const noexcept { return _path; }
std::uintmax_t CPK::directory_entry::file_size() const { return decompressed_size; }

CPK::CPK(std::filesystem::path p)
	: _filepath(std::move(p))
	, _buffer({}) {
	unpack(std::ifstream(_filepath, std::ios::binary));
}

CPK::CPK(CPK&& other) noexcept
	: _files({})
	, _filepath(other._filepath) {
	std::swap(_files, other._files);
}

bool CPK::directory_entry::operator<(const directory_entry& e) const noexcept { return id < e.id; }

const std::vector<char>& CPK::extract(const directory_entry& e) const {
	if (!extract(e, _buffer)) {
		_buffer.clear();
	}
	return _buffer;
}
bool CPK::extract(const directory_entry& e, std::vector<char>& out) const {
	auto ifs = std::ifstream(_filepath, std::ios::binary);
	out.resize(e.decompressed_size);
	ifs.seekg(e.offset, std::ios::beg);
	if (e.compressed_size == e.decompressed_size) {
		ifs.read(out.data(), e.compressed_size);
		return true;
	}
	_buffer.resize(e.compressed_size);
	if (!ifs.read(_buffer.data(), e.compressed_size)) {
		return false;
	}
	// TODO: return value kinda useless
	const int64_t ret = decompress(_buffer, out);
	return true;
}

CPK::iterator CPK::begin() noexcept { return _files.begin(); }
CPK::iterator CPK::end() noexcept { return _files.end(); }
CPK::const_iterator CPK::begin() const noexcept { return _files.begin(); }
CPK::const_iterator CPK::end() const noexcept { return _files.end(); }
CPK::const_iterator CPK::cbegin() const noexcept { return _files.cbegin(); }
CPK::const_iterator CPK::cend() const noexcept { return _files.cend(); }

std::vector<CPK::directory_entry>::const_iterator CPK::at_id(uint32_t id) const noexcept {
	return std::find_if(_files.begin(), _files.end(), [id](const directory_entry& e) { return e.id == id; });
}

CPK::const_iterator CPK::by_name(const std::filesystem::path& p) const {
	return std::find_if(_files.begin(), _files.end(), [p](const directory_entry& e) { return e._path == p; });
}

std::vector<CPK::directory_entry> generate_TOC(const UTF& utf, uint64_t offset = 0) {
	// MUST have these fields
	auto dirnames = utf.find("DirName");
	auto filenames = utf.find("FileName");
	auto filesizes = utf.find("FileSize");
	auto extractsizes = utf.find("ExtractSize");
	auto fileoffsets = utf.find("FileOffset");
	auto ids = utf.find("ID");

	auto has = [&utf](std::vector<UTF::field>::const_iterator it) { return it != utf.end(); };
	if (!(has(dirnames) && has(filenames) && has(filesizes) && has(extractsizes) && has(fileoffsets) && has(ids))) {
		return {};
	}
	auto size = [](std::vector<UTF::field>::const_iterator it) { return (*it).values.size(); };
	if (!are_equal(
			size(dirnames), size(filenames), size(filesizes), size(extractsizes), size(fileoffsets), size(ids))) {
		return {};
	}

	const size_t idsize = size(ids);
	std::vector<CPK::directory_entry> ret(idsize);
	for (size_t i = 0; i < idsize; ++i) {
		auto& directory_entry = ret[i];
		directory_entry.id = ids->cast_at<uint32_t>(i).value_or(0);
		directory_entry.offset = fileoffsets->cast_at<uint64_t>(i).value_or(0) + offset;
		directory_entry.compressed_size = filesizes->cast_at<uint64_t>(i).value_or(0);
		directory_entry.decompressed_size = extractsizes->cast_at<uint64_t>(i).value_or(0);
		const auto n = std::get<std::string>(filenames->values[i]);
		const auto p = std::get<std::string>(dirnames->values[i]);
		directory_entry._path = std::filesystem::path(p) / n;
	}
	std::sort(ret.begin(), ret.end());
	return ret;
}

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

void CPK::unpack(std::istream&& is) {
	uint64_t size = read_header(is, CPK_magic);
	_buffer.resize(size, 0);
	is.read(_buffer.data(), size);
	auto utf_table = UTF(std::move(_buffer));
	// auto utf_table = UTF(io::isubstream(is, size));

	if (auto iter = utf_table.find("TocOffset"); iter != utf_table.end() && iter->valid) {
		const auto offset = iter->cast_at<uint64_t>(0).value();
		is.seekg(offset, std::ios::beg);
		size = read_header(is, TOC_magic);

		_buffer.resize(size);
		is.read(_buffer.data(), size);
		auto toc_table = UTF(std::move(_buffer));
		// auto toc_table = UTF(io::isubstream(is, size));

		_files = generate_TOC(toc_table, offset);
	}
}
