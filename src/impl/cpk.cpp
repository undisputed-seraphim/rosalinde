#include "cpk.hpp"
#include "endian_swap.hpp"
#include "streams.hpp"
#include "utf.hpp"
#include "utils.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>

constexpr uint32_t CPK_magic = 0x204B5043; // "CPK " reversed endian
constexpr uint32_t TOC_magic = 0x20434F54; // "TOC " reversed endian

// crilayla.cpp
int64_t compress(const std::vector<char>& src, std::vector<char>& dst);
int64_t decompress(std::span<char> in, std::vector<char>& out);

CPK::CPK(std::string_view path)
	: _filepath(std::string(path))
	, _buffer({}) {
	unpack(std::ifstream(_filepath, std::ios::binary));
}

CPK::CPK(CPK&& other) noexcept
	: _files({})
	, _filepath(other._filepath) {
	std::swap(_files, other._files);
}

bool CPK::entry::operator<(const entry& e) const noexcept { return id < e.id; }

bool CPK::extract(const entry& e, std::vector<char>& out) const {
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

std::vector<CPK::entry>::const_iterator CPK::at_id(uint32_t id) const noexcept {
	return std::find_if(_files.begin(), _files.end(), [id](const entry& e) { return e.id == id; });
}

std::vector<CPK::entry>::const_iterator CPK::by_name(std::string_view name, std::string_view path) const noexcept {
	return std::find_if(_files.begin(), _files.end(), [name, path](const entry& e) {
		if (name == "*") {
			return (e.path == path);
		}
		return (e.name == name) && (e.path == path);
	});
}

std::vector<CPK::entry>::const_iterator CPK::by_name(std::string_view fullname) const noexcept {
	auto path = fullname.substr(0, fullname.find_last_of('/'));
	auto name = fullname.substr(fullname.find_last_of('/') + 1);
	return by_name(name, path);
}

std::vector<CPK::entry> generate_TOC(const UTF& utf, uint64_t offset = 0) {
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
	std::vector<CPK::entry> ret(idsize);
	for (size_t i = 0; i < idsize; ++i) {
		auto& entry = ret[i];
		entry.id = ids->cast_at<uint32_t>(i).value_or(0);
		entry.offset = fileoffsets->cast_at<uint64_t>(i).value_or(0) + offset;
		entry.compressed_size = filesizes->cast_at<uint64_t>(i).value_or(0);
		entry.decompressed_size = extractsizes->cast_at<uint64_t>(i).value_or(0);
		entry.name = std::get<std::string>(filenames->values[i]);
		entry.path = std::get<std::string>(dirnames->values[i]);
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
	//auto utf_table = UTF(io::isubstream(is, size));

	if (auto iter = utf_table.find("TocOffset"); iter != utf_table.end() && iter->valid) {
		const auto offset = iter->cast_at<uint64_t>(0).value();
		is.seekg(offset, std::ios::beg);
		size = read_header(is, TOC_magic);

		_buffer.resize(size);
		is.read(_buffer.data(), size);
		auto toc_table = UTF(std::move(_buffer));
		//auto toc_table = UTF(io::isubstream(is, size));

		_files = generate_TOC(toc_table, offset);
	}
}
