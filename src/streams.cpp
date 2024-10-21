#include "streams.hpp"

namespace io {

substreambuf::substreambuf(std::istream& is, std::streamsize size)
	: _sbuf(is.rdbuf())
	, _size(size)
	, _start(is.tellg()) {}

substreambuf::substreambuf(std::streambuf* sbuf, std::streamsize size)
	: _sbuf(sbuf)
	, _size(size)
	, _start(sbuf->pubseekoff(0, std::ios::cur)) {}

void substreambuf::imbue(const std::locale& loc) { _sbuf->pubimbue(loc); }
std::streambuf* substreambuf::setbuf(char_type* s, std::streamsize n) { return _sbuf->pubsetbuf(s, n); }

substreambuf::pos_type
substreambuf::seekoff(off_type off, const std::ios::seekdir dir, const std::ios::openmode which) {
	if (dir == std::ios::cur) {
		return _sbuf->pubseekoff(off, dir, which) - _start;
	} else if (dir == std::ios::beg) {
		off += _start;
	} else if (dir == std::ios::end) {
		off += _start + _size;
	}
	return _sbuf->pubseekpos(pos_type{off}, which) - _start;
}

substreambuf::pos_type substreambuf::seekpos(pos_type pos, const std::ios::openmode which) {
	return _sbuf->pubseekpos(pos + _start, which) - _start;
}

substreambuf::int_type substreambuf::underflow() {
	auto c = _sbuf->sgetc();
	if (_sbuf->pubseekoff(0, std::ios::cur) >= _start + _size) {
		return traits_type::eof();
	}
	return c;
}
substreambuf::int_type substreambuf::uflow() {
	auto c = _sbuf->sbumpc();
	if (_sbuf->pubseekoff(0, std::ios::cur) >= _start + _size) {
		return traits_type::eof();
	}
	return c;
}
std::streamsize substreambuf::xsgetn(char_type* s, std::streamsize count) { return _sbuf->sgetn(s, count); }
std::streamsize substreambuf::xsputn(const char_type* s, std::streamsize count) { return _sbuf->sputn(s, count); }

} // namespace io