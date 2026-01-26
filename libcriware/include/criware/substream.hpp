#pragma once

#include <istream>
#include <streambuf>

template <typename CharT, typename Traits = std::char_traits<CharT>>
class basic_substreambuf : public std::basic_streambuf<CharT, Traits> {
public:
	using char_type = CharT;
	using traits_type = Traits;
	using int_type = traits_type::int_type;
	using pos_type = traits_type::pos_type;
	using off_type = traits_type::off_type;

protected:
	using base_type = std::basic_streambuf<CharT>;

	base_type* _underlying;
	pos_type _start;
	pos_type _end;
	pos_type _current;
	char_type _buffer;

	using base_type::egptr;
	using base_type::gbump;
	using base_type::gptr;
	using base_type::setg;
	using base_type::setp;

	int_type underflow() override {
		if (!_underlying) {
			return traits_type::eof();
		}

		if (_current >= _end) {
			return traits_type::eof();
		}

		if (_underlying->pubseekpos(_current, std::ios::in) == pos_type(-1)) {
			return traits_type::eof();
		}

		int_type ch = _underlying->sgetc();
		if (traits_type::eq_int_type(ch, traits_type::eof())) {
			return traits_type::eof();
		}

		pos_type next_pos = _current;
		if (next_pos >= _end) {
			return traits_type::eof();
		}

		_buffer = traits_type::to_char_type(ch);
		this->setg(&_buffer, &_buffer, &_buffer + 1);

		return ch;
	}

	std::streamsize xsgetn(char_type* s, std::streamsize count) override {
		if (!_underlying || count <= 0) {
			return 0;
		}
		std::streamsize total_read = 0;
		while (count > 0) {
			if (gptr() < egptr()) {
				std::streamsize avail = egptr() - gptr();
				std::streamsize to_copy = std::min(avail, count);
				std::copy(gptr(), gptr() + to_copy, s);
				gbump(static_cast<int>(to_copy));
				s += to_copy;
				count -= to_copy;
				total_read += to_copy;
				_current += to_copy;
				continue;
			}

			int_type ch = underflow();
			if (traits_type::eq_int_type(ch, traits_type::eof())) {
				break;
			}

			const auto remaining = static_cast<off_type>(_end - _current);
			const auto to_read = static_cast<std::streamsize>(std::min<off_type>(remaining, count));
			if (_underlying->pubseekpos(_current, std::ios::in) == pos_type(-1)) {
				break;
			}

			std::streamsize got = _underlying->sgetn(s, to_read);
			if (got <= 0) {
				break;
			}

			s += got;
			count += got;
			total_read += got;
			_current += got;

			setg(nullptr, nullptr, nullptr);
		}
		return total_read;
	}

	pos_type
	seekoff(off_type off, std::ios::seekdir dir, std::ios::openmode which = std::ios::in | std::ios::out) override {
		if (!_underlying) {
			return pos_type(-1);
		}

		pos_type new_abs;

		switch (dir) {
		case std::ios::beg:
			new_abs = _start + off;
			break;
		case std::ios::cur:
			new_abs = _current + off;
			break;
		case std::ios::end:
			new_abs = _end + off;
			break;
		default:
			return pos_type(-1);
		}

		if (new_abs < _start || new_abs > _end) {
			return pos_type(-1);
		}

		if ((which & std::ios::in) || (which & std::ios::out)) {
			if (_underlying->pubseekpos(new_abs, which) == pos_type(-1)) {
				return pos_type(-1);
			}
		}

		_current = new_abs;
		setg(nullptr, nullptr, nullptr);
		setp(nullptr, nullptr);

		return _current - _start;
	}

	pos_type seekpos(pos_type pos, std::ios::openmode which = std::ios::in | std::ios::out) override {
		return seekoff(static_cast<off_type>(pos), std::ios::beg, which);
	}

	int sync() override { return _underlying ? _underlying->pubsync() : -1; }

public:
	basic_substreambuf(base_type* underlying, pos_type start, pos_type end) noexcept
		: _underlying(underlying)
		, _start(start)
		, _end(end)
		, _current(start) {
		_ASSERT(_underlying);
		this->setg(nullptr, nullptr, nullptr);
	}

	basic_substreambuf()
		: basic_substreambuf(nullptr, 0, 0) {}

	basic_substreambuf(const basic_substreambuf&) = delete;

	basic_substreambuf(basic_substreambuf&& other) noexcept
		: _underlying(other._underlying)
		, _start(other._start)
		, _end(other._end)
		, _current(other._current)
		, _buffer(other._buffer) {
		other._underlying = nullptr;
	}

	basic_substreambuf& operator=(basic_substreambuf&&) noexcept = default;
};

using substreambuf = basic_substreambuf<char>;
// using wsubstreambuf = basic_substreambuf<wchar_t>;

class utf_streambuf final : public std::streambuf {
public:
	using base_type = std::streambuf;
	using char_type = base_type::char_type;
	using traits_type = base_type::traits_type;
	using int_type = traits_type::int_type;
	using pos_type = traits_type::pos_type;
	using off_type = traits_type::off_type;

protected:
	static constexpr uint32_t j0 = 0x655F;
	static constexpr uint32_t t = 0x4115;

	base_type* _underlying;
	uint32_t _j;
	char_type _buffer[4096];

	char decrypt(int_type c) noexcept {
		c ^= (_j & 0xFF);
		_j *= t;
		return static_cast<char>(c);
	}

	int_type* decrypt(int_type* cs, size_t s) noexcept {
		for (; s > 0; --s) {
			cs[s] = decrypt(cs[s]);
		}
		return cs;
	}

	int_type underflow() override {
		if (!_underlying) {
			return traits_type::eof();
		}
		std::streamsize n = _underlying->sgetn(_buffer, sizeof(_buffer));
		if (n <= 0) {
			return traits_type::eof();
		}

		for (size_t i = 0; i < static_cast<size_t>(n); ++i) {
			uint32_t c = static_cast<uint32_t>(_buffer[i]);
			c ^= (_j & 0xFF);
			_buffer[i] = static_cast<char_type>(c);
			_j *= t;
		}

		setg(_buffer, _buffer, _buffer + n);
		return traits_type::to_int_type(*gptr());
	}

public:
	utf_streambuf(base_type* underlying)
		: _underlying(nullptr)
		, _j(j0) {}

	utf_streambuf(const utf_streambuf&) = delete;
};