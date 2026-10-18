#include <criware/cpk.hpp>

#include <cstring>
#include <fstream>
#include <istream>
#include <limits>
#include <stdexcept>

#if !defined(__EMSCRIPTEN__) && !defined(__wasm__)
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <system_error>
#endif

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

#if !defined(__EMSCRIPTEN__) && !defined(__wasm__)
class mapped_output {
public:
	mapped_output(const std::filesystem::path& path, uint64_t size)
		: _path(path)
		, _size(size) {
#if defined(_WIN32)
		_file = CreateFileW(
			_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (_file == INVALID_HANDLE_VALUE) {
			fail("CreateFileW", false);
		}
		LARGE_INTEGER end;
		end.QuadPart = static_cast<LONGLONG>(_size);
		if (!SetFilePointerEx(_file, end, nullptr, FILE_BEGIN) || !SetEndOfFile(_file)) {
			fail("SetEndOfFile");
		}
		if (_size != 0) {
			_mapping = CreateFileMappingW(
				_file, nullptr, PAGE_READWRITE, static_cast<DWORD>(_size >> 32), static_cast<DWORD>(_size), nullptr);
			if (!_mapping) {
				fail("CreateFileMappingW");
			}
			_data = MapViewOfFile(_mapping, FILE_MAP_WRITE, 0, 0, 0);
			if (!_data) {
				fail("MapViewOfFile");
			}
			_mapped = true;
		}
#else
		_fd = ::open(_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
		if (_fd < 0) {
			fail("open", false);
		}
		if (::ftruncate(_fd, static_cast<off_t>(_size)) != 0) {
			fail("ftruncate");
		}
		if (_size != 0) {
			_data = ::mmap(nullptr, static_cast<size_t>(_size), PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
			if (_data == MAP_FAILED) {
				_data = nullptr;
				fail("mmap");
			}
			_mapped = true;
		}
#endif
	}

	~mapped_output() { close(); }

	mapped_output(const mapped_output&) = delete;
	mapped_output& operator=(const mapped_output&) = delete;

	std::span<char> span() { return {static_cast<char*>(_data), static_cast<size_t>(_size)}; }

	void flush() {
#if defined(_WIN32)
		if (_mapped && !FlushViewOfFile(_data, static_cast<SIZE_T>(_size))) {
			fail("FlushViewOfFile");
		}
		if (_file != INVALID_HANDLE_VALUE && !FlushFileBuffers(_file)) {
			fail("FlushFileBuffers");
		}
#else
		if (_mapped && ::msync(_data, static_cast<size_t>(_size), MS_SYNC) != 0) {
			fail("msync");
		}
#endif
	}

	void discard() {
		close();
		std::error_code ec;
		std::filesystem::remove(_path, ec);
	}

private:
	void close() {
#if defined(_WIN32)
		if (_mapped) {
			UnmapViewOfFile(_data);
			_mapped = false;
			_data = nullptr;
		}
		if (_mapping) {
			CloseHandle(_mapping);
			_mapping = nullptr;
		}
		if (_file != INVALID_HANDLE_VALUE) {
			CloseHandle(_file);
			_file = INVALID_HANDLE_VALUE;
		}
#else
		if (_mapped) {
			::munmap(_data, static_cast<size_t>(_size));
			_mapped = false;
			_data = nullptr;
		}
		if (_fd >= 0) {
			::close(_fd);
			_fd = -1;
		}
#endif
	}

	[[noreturn]] void fail(const char* what, bool remove_file = true) {
#if defined(_WIN32)
		const DWORD err = GetLastError();
#else
		const int err = errno;
#endif
		close();
		if (remove_file) {
			std::error_code ec;
			std::filesystem::remove(_path, ec);
		}
#if defined(_WIN32)
		throw std::system_error(static_cast<int>(err), std::system_category(), what);
#else
		throw std::system_error(err, std::generic_category(), what);
#endif
	}

	std::filesystem::path _path;
	uint64_t _size;
	void* _data = nullptr;
	bool _mapped = false;
#if defined(_WIN32)
	HANDLE _file = INVALID_HANDLE_VALUE;
	HANDLE _mapping = nullptr;
#else
	int _fd = -1;
#endif
};
#endif

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

void CPKTable::extract_to_file(const row_type& entry, const std::filesystem::path& path) const {
	const uint64_t file_size = get<"FileSize">(entry);
	const uint64_t extract_size = get<"ExtractSize">(entry);
	const uint64_t file_offset = get<"FileOffset">(entry);
	if (file_size > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
		extract_size > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
		file_size > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()) ||
		extract_size > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max())) {
		throw std::runtime_error("CPK entry is too large");
	}

#if defined(__EMSCRIPTEN__) || defined(__wasm__)
	// TODO: stream to OPFS/WASI instead of materializing the whole output.
	std::vector<char> data;
	extract(entry, data);
	std::ofstream ofs(path, std::ios::binary);
	if (!ofs) {
		throw std::runtime_error("Failed to open " + path.string());
	}
	ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
	if (!ofs) {
		throw std::runtime_error("Failed to write " + path.string());
	}
#else
	std::ifstream ifs(_path, std::ios::binary);
	if (!ifs) {
		throw std::runtime_error("Failed to open " + _path.string());
	}
	ifs.seekg(static_cast<std::streamoff>(file_offset + _offset), std::ios::beg);
	if (!ifs) {
		throw std::runtime_error("Failed to seek in " + _path.string());
	}

	mapped_output out(path, extract_size);
	auto dst = out.span();
	if (extract_size == file_size) {
		if (extract_size != 0) {
			ifs.read(dst.data(), static_cast<std::streamsize>(extract_size));
			if (!ifs) {
				out.discard();
				throw std::runtime_error("Failed to read " + _path.string());
			}
		}
	} else {
		const auto ret = decompress(ifs, file_offset + _offset, file_size, dst);
		if (ret < 0) {
			out.discard();
			throw std::runtime_error("Failed to decompress " + _path.string());
		}
	}

	try {
		out.flush();
	} catch (...) {
		out.discard();
		throw;
	}
#endif
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
