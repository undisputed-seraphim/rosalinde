#pragma once

#include <algorithm>
#include <istream>
#include <streambuf>
#include <spanstream>
#include <sstream>

namespace io {

class substreambuf final : public std::streambuf {
private:
	std::streambuf* _sbuf;
	std::streamsize _size;
	std::streampos _start;

public:
	substreambuf(std::istream& is, std::streamsize size);
	substreambuf(std::streambuf* sbuf, std::streamsize size);
	substreambuf(const substreambuf&) = delete;
	substreambuf(substreambuf&&) = default;

	std::streampos underlying_offset() { return _sbuf->pubseekoff(0, std::ios::cur); }
	std::streamsize size() const noexcept { return _size; }

	substreambuf& operator=(substreambuf&& other) noexcept = default;

protected:
	void imbue(const std::locale& loc) override;
	std::streambuf* setbuf(char_type* s, std::streamsize n) override;
	pos_type seekoff(off_type off, const std::ios::seekdir dir, const std::ios::openmode which) override;
	pos_type seekpos(pos_type pos, const std::ios::openmode which) override;
	int_type underflow() override;
	int_type uflow() override;
	std::streamsize xsgetn(char_type* s, std::streamsize count) override;
	std::streamsize xsputn(const char_type* s, std::streamsize count) override;
};

class isubstream : public std::istream {
	substreambuf _sbuf;

public:
	isubstream(std::istream& is, std::streamsize size)
		: std::istream(&_sbuf)
		, _sbuf(is, size) {}
	isubstream(const isubstream&) = delete;
	isubstream(isubstream&&) = default;

	std::streampos utellg() { return _sbuf.underlying_offset(); }

	isubstream& operator=(isubstream&& other) noexcept = default;
};

template <typename T>
static T read(std::istream& is) {
	T val;
	is.read((char*)&val, sizeof(val));
	return val;
}

template <>
static char read<char>(std::istream& is) {
	char val;
	is.get(val);
	return val;
}

} // namespace io