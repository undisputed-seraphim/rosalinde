#include "line_editor.hpp"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(_WIN32)
#include <conio.h>
#include <io.h>
#elif defined(__EMSCRIPTEN__) || defined(__wasm__)
#else
#include <cerrno>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace cli {
namespace {

enum class key_kind {
	character,
	enter,
	backspace,
	del,
	left,
	right,
	up,
	down,
	home,
	end,
	page_up,
	page_down,
	ctrl_a,
	ctrl_e,
	ctrl_u,
	ctrl_k,
	ctrl_w,
	ctrl_l,
	ctrl_c,
	ctrl_d,
	tab,
	escape,
	eof,
	ignored
};

struct key {
	key_kind kind = key_kind::ignored;
	unsigned char character = 0;
};

int first_number(std::string_view text) {
	int value = 0;
	bool found = false;
	for (char c : text) {
		if (c >= '0' && c <= '9') {
			found = true;
			value = value * 10 + (c - '0');
		} else {
			break;
		}
	}
	return found ? value : -1;
}

} // namespace

struct line_editor::impl {
	bool stderr_tty = false;

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && !defined(__wasm__)
	termios saved{};
	bool raw = false;
#endif

	impl() {
#if defined(_WIN32)
		stderr_tty = _isatty(_fileno(stderr)) != 0;
#elif defined(__EMSCRIPTEN__) || defined(__wasm__)
		stderr_tty = false;
#else
		stderr_tty = ::isatty(STDERR_FILENO) != 0;
		if (tcgetattr(STDIN_FILENO, &saved) != 0)
			throw std::runtime_error("cannot read terminal mode");

		termios raw_mode = saved;
		raw_mode.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
		raw_mode.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
		raw_mode.c_cc[VMIN] = 1;
		raw_mode.c_cc[VTIME] = 0;
		if (tcsetattr(STDIN_FILENO, TCSANOW, &raw_mode) != 0)
			throw std::runtime_error("cannot set terminal mode");
		raw = true;
#endif

		if (stderr_tty) {
			std::cerr << "\x1b[?9l\x1b[?1000l\x1b[?1002l\x1b[?1003l\x1b[?1005l\x1b[?1006l\x1b[?1015l" << std::flush;
		}
	}

	~impl() { restore(); }

	void restore() {
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && !defined(__wasm__)
		if (raw) {
			tcsetattr(STDIN_FILENO, TCSANOW, &saved);
			raw = false;
		}
#endif
	}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && !defined(__wasm__)
	int read_byte() const {
		unsigned char byte = 0;
		while (true) {
			const auto count = ::read(STDIN_FILENO, &byte, 1);
			if (count == 1)
				return byte;
			if (count < 0 && errno == EINTR)
				continue;
			return -1;
		}
	}

	int read_byte_timeout(int milliseconds) const {
		pollfd descriptor{STDIN_FILENO, POLLIN, 0};
		while (true) {
			const int result = ::poll(&descriptor, 1, milliseconds);
			if (result < 0 && errno == EINTR)
				continue;
			if (result <= 0)
				return -1;
			return read_byte();
		}
	}

	key parse_csi() const {
		std::string parameters;
		int final = 0;
		while (true) {
			const int c = read_byte();
			if (c < 0)
				return {};
			if (c >= 0x40 && c <= 0x7e) {
				final = c;
				break;
			}
			parameters.push_back(static_cast<char>(c));
		}

		if (final == 'M' && parameters.empty()) {
			for (int i = 0; i < 3; ++i)
				read_byte();
			return {};
		}
		if (final == 'M' || final == 'm')
			return {};

		switch (final) {
		case 'A':
			return {key_kind::up};
		case 'B':
			return {key_kind::down};
		case 'C':
			return {key_kind::right};
		case 'D':
			return {key_kind::left};
		case 'H':
			return {key_kind::home};
		case 'F':
			return {key_kind::end};
		case '~':
			switch (first_number(parameters)) {
			case 1:
			case 7:
				return {key_kind::home};
			case 3:
				return {key_kind::del};
			case 4:
			case 8:
				return {key_kind::end};
			case 5:
				return {key_kind::page_up};
			case 6:
				return {key_kind::page_down};
			default:
				return {};
			}
		default:
			return {};
		}
	}

	key read_key() const {
		const int c = read_byte();
		if (c < 0)
			return {key_kind::eof};

		if (c == 0x1b) {
			const int next = read_byte_timeout(30);
			if (next < 0)
				return {key_kind::escape};
			if (next == '[')
				return parse_csi();
			if (next == 'O') {
				const int final = read_byte();
				switch (final) {
				case 'A':
					return {key_kind::up};
				case 'B':
					return {key_kind::down};
				case 'C':
					return {key_kind::right};
				case 'D':
					return {key_kind::left};
				case 'H':
					return {key_kind::home};
				case 'F':
					return {key_kind::end};
				default:
					return {};
				}
			}
			return {};
		}

		switch (c) {
		case '\r':
		case '\n':
			return {key_kind::enter};
		case 0x7f:
		case 0x08:
			return {key_kind::backspace};
		case 0x01:
			return {key_kind::ctrl_a};
		case 0x03:
			return {key_kind::ctrl_c};
		case 0x04:
			return {key_kind::ctrl_d};
		case 0x05:
			return {key_kind::ctrl_e};
		case 0x09:
			return {key_kind::tab};
		case 0x0b:
			return {key_kind::ctrl_k};
		case 0x0c:
			return {key_kind::ctrl_l};
		case 0x15:
			return {key_kind::ctrl_u};
		case 0x17:
			return {key_kind::ctrl_w};
		default:
			if (c >= 0x20)
				return {key_kind::character, static_cast<unsigned char>(c)};
			return {};
		}
	}
#elif defined(_WIN32)
	key read_key() const {
		const int c = _getch();
		if (c == 0 || c == 224) {
			switch (_getch()) {
			case 72:
				return {key_kind::up};
			case 80:
				return {key_kind::down};
			case 75:
				return {key_kind::left};
			case 77:
				return {key_kind::right};
			case 71:
				return {key_kind::home};
			case 79:
				return {key_kind::end};
			case 83:
				return {key_kind::del};
			case 73:
				return {key_kind::page_up};
			case 81:
				return {key_kind::page_down};
			default:
				return {};
			}
		}

		switch (c) {
		case '\r':
		case '\n':
			return {key_kind::enter};
		case 8:
		case 127:
			return {key_kind::backspace};
		case 1:
			return {key_kind::ctrl_a};
		case 3:
			return {key_kind::ctrl_c};
		case 4:
			return {key_kind::ctrl_d};
		case 5:
			return {key_kind::ctrl_e};
		case 9:
			return {key_kind::tab};
		case 11:
			return {key_kind::ctrl_k};
		case 12:
			return {key_kind::ctrl_l};
		case 21:
			return {key_kind::ctrl_u};
		case 23:
			return {key_kind::ctrl_w};
		default:
			if (c >= 32)
				return {key_kind::character, static_cast<unsigned char>(c)};
			return {};
		}
	}
#else
	key read_key() const { return {key_kind::eof}; }
#endif
};

line_editor::line_editor()
	: _impl(std::make_unique<impl>()) {}

line_editor::~line_editor() = default;

std::optional<std::string> line_editor::read_line(std::string_view prompt, const std::deque<std::string>& history) {
	std::string buffer;
	size_t cursor = 0;
	int history_position = -1;
	std::string draft;

	auto redraw = [&] {
		if (!_impl->stderr_tty)
			return;
		std::cerr << '\r' << "\x1b[2K" << prompt << buffer;
		if (cursor < buffer.size())
			std::cerr << "\x1b[" << (buffer.size() - cursor) << 'D';
		std::cerr << std::flush;
	};

	redraw();
	while (true) {
		const key input = _impl->read_key();
		switch (input.kind) {
		case key_kind::character:
			buffer.insert(buffer.begin() + static_cast<std::ptrdiff_t>(cursor), static_cast<char>(input.character));
			++cursor;
			redraw();
			break;

		case key_kind::enter:
			if (_impl->stderr_tty)
				std::cerr << '\n';
			return buffer;

		case key_kind::eof:
			if (_impl->stderr_tty)
				std::cerr << '\n';
			return std::nullopt;

		case key_kind::backspace:
			if (cursor != 0) {
				buffer.erase(cursor - 1, 1);
				--cursor;
				redraw();
			}
			break;

		case key_kind::del:
			if (cursor < buffer.size()) {
				buffer.erase(cursor, 1);
				redraw();
			}
			break;

		case key_kind::left:
			if (cursor != 0) {
				--cursor;
				redraw();
			}
			break;

		case key_kind::right:
			if (cursor < buffer.size()) {
				++cursor;
				redraw();
			}
			break;

		case key_kind::home:
			cursor = 0;
			redraw();
			break;

		case key_kind::end:
			cursor = buffer.size();
			redraw();
			break;

		case key_kind::up:
			if (history.empty())
				break;
			if (history_position < 0) {
				draft = buffer;
				history_position = 0;
			} else if (static_cast<size_t>(history_position + 1) < history.size()) {
				++history_position;
			} else {
				break;
			}
			buffer = history[static_cast<size_t>(history_position)];
			cursor = buffer.size();
			redraw();
			break;

		case key_kind::down:
			if (history_position < 0)
				break;
			if (history_position == 0) {
				history_position = -1;
				buffer = draft;
			} else {
				--history_position;
				buffer = history[static_cast<size_t>(history_position)];
			}
			cursor = buffer.size();
			redraw();
			break;

		case key_kind::ctrl_a:
			cursor = 0;
			redraw();
			break;

		case key_kind::ctrl_e:
			cursor = buffer.size();
			redraw();
			break;

		case key_kind::ctrl_u:
			buffer.erase(0, cursor);
			cursor = 0;
			redraw();
			break;

		case key_kind::ctrl_k:
			buffer.erase(cursor);
			redraw();
			break;

		case key_kind::ctrl_w: {
			size_t start = cursor;
			while (start > 0 && std::isspace(static_cast<unsigned char>(buffer[start - 1])))
				--start;
			while (start > 0 && !std::isspace(static_cast<unsigned char>(buffer[start - 1])))
				--start;
			buffer.erase(start, cursor - start);
			cursor = start;
			redraw();
			break;
		}

		case key_kind::ctrl_l:
			if (_impl->stderr_tty) {
				std::cerr << "\x1b[2J\x1b[H";
				redraw();
			}
			break;

		case key_kind::tab:
			buffer.insert(buffer.begin() + static_cast<std::ptrdiff_t>(cursor), ' ');
			++cursor;
			redraw();
			break;

		case key_kind::ctrl_c:
			if (_impl->stderr_tty)
				std::cerr << "^C\n";
			return std::string();

		case key_kind::ctrl_d:
			if (buffer.empty()) {
				if (_impl->stderr_tty)
					std::cerr << '\n';
				return std::nullopt;
			}
			if (cursor < buffer.size()) {
				buffer.erase(cursor, 1);
				redraw();
			}
			break;

		case key_kind::page_up:
		case key_kind::page_down:
		case key_kind::escape:
		case key_kind::ignored:
			break;
		}
	}
}

} // namespace cli
