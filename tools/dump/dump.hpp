#pragma once

#include <criware/cpk.hpp>

#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace dump {

struct options {
	bool json = false;
	bool full = false;
	bool limit_set = false;
	size_t limit = 10;
	bool timeline = false;
	bool colors = false;
	bool attributes = false;
	std::string_view section;
	std::optional<size_t> track;
	std::optional<size_t> sequence;
};

struct context {
	std::string_view path;
	const cpk::entry& item;
	options opt;
};

class json_writer {
public:
	explicit json_writer(std::ostream& out)
		: _out(out) {}

	void begin_object() { open('{', false); }
	void end_object() { close('}'); }
	void begin_array() { open('[', true); }
	void end_array() { close(']'); }

	void begin_object(std::string_view name) {
		key(name);
		begin_object();
	}

	void begin_array(std::string_view name) {
		key(name);
		begin_array();
	}

	void key(std::string_view name) {
		if (_stack.empty() || _stack.back().array)
			throw std::logic_error("JSON key outside object");
		auto& frame = _stack.back();
		if (!frame.first)
			_out << ',';
		_out << '\n';
		indent(_stack.size());
		write_string(name);
		_out << ": ";
		frame.first = false;
		frame.after_key = true;
	}

	void string(std::string_view value) {
		before_value();
		write_string(value);
	}

	void null() {
		before_value();
		_out << "null";
	}

	void boolean(bool value) {
		before_value();
		_out << (value ? "true" : "false");
	}

	void number_unsigned(uint64_t value) {
		before_value();
		std::format_to(std::ostreambuf_iterator<char>(_out), "{}", value);
	}

	void number_signed(int64_t value) {
		before_value();
		std::format_to(std::ostreambuf_iterator<char>(_out), "{}", value);
	}

	void number(double value) {
		if (!std::isfinite(value)) {
			null();
			return;
		}
		before_value();
		std::format_to(std::ostreambuf_iterator<char>(_out), "{}", value);
	}

	template <std::integral T>
		requires(!std::same_as<T, bool>)
	void number(T value) {
		if constexpr (std::is_signed_v<T>)
			number_signed(static_cast<int64_t>(value));
		else
			number_unsigned(static_cast<uint64_t>(value));
	}

	template <std::floating_point T>
	void number(T value) {
		number(static_cast<double>(value));
	}

	void field(std::string_view name, std::string_view value) {
		key(name);
		string(value);
	}

	void field(std::string_view name, const std::string& value) { field(name, std::string_view(value)); }
	void field(std::string_view name, const char* value) { field(name, std::string_view(value)); }
	void field(std::string_view name, bool value) {
		key(name);
		boolean(value);
	}

	template <std::integral T>
		requires(!std::same_as<T, bool>)
	void field(std::string_view name, T value) {
		key(name);
		number(value);
	}

	template <std::floating_point T>
	void field(std::string_view name, T value) {
		key(name);
		number(value);
	}

private:
	struct frame {
		bool array;
		bool first = true;
		bool after_key = false;
	};

	void open(char brace, bool array) {
		before_value();
		_out << brace;
		_stack.push_back({array, true, false});
	}

	void close(char brace) {
		if (_stack.empty())
			throw std::logic_error("JSON close without open");
		const auto frame = _stack.back();
		_stack.pop_back();
		if (!frame.first) {
			_out << '\n';
			indent(_stack.size());
		}
		_out << brace;
	}

	void before_value() {
		if (_stack.empty()) {
			if (_root_written)
				throw std::logic_error("multiple JSON roots");
			_root_written = true;
			return;
		}

		auto& frame = _stack.back();
		if (frame.after_key) {
			frame.after_key = false;
			return;
		}
		if (!frame.first)
			_out << ',';
		_out << '\n';
		indent(_stack.size());
		frame.first = false;
	}

	void write_string(std::string_view value) {
		_out << '"';
		for (unsigned char c : value) {
			switch (c) {
			case '"':
				_out << "\\\"";
				break;
			case '\\':
				_out << "\\\\";
				break;
			case '\b':
				_out << "\\b";
				break;
			case '\f':
				_out << "\\f";
				break;
			case '\n':
				_out << "\\n";
				break;
			case '\r':
				_out << "\\r";
				break;
			case '\t':
				_out << "\\t";
				break;
			default:
				if (c < 0x20)
					std::format_to(std::ostreambuf_iterator<char>(_out), "\\u{:04x}", c);
				else
					_out << static_cast<char>(c);
			}
		}
		_out << '"';
	}

	void indent(size_t depth) {
		for (size_t i = 0; i < depth; ++i)
			_out << "  ";
	}

	std::ostream& _out;
	std::vector<frame> _stack;
	bool _root_written = false;
};

int dispatch(const cpk::archive& archive, const cpk::entry& item, std::string_view path, const options& opt);

std::string lower_extension(std::string_view name);
void write_metadata_text(const context& ctx, std::string_view type);
void write_metadata_json(json_writer& json, const context& ctx, std::string_view type);
void print_hex_preview(std::span<const char> data, size_t max_bytes = 64);

int dump_acb(const context& ctx, std::span<char> data);
int dump_nnd(const context& ctx, std::span<char> data);
int dump_ftx(const context& ctx, std::span<char> data);
int dump_mbs(const context& ctx, std::span<char> data);
int dump_bbs(const context& ctx, std::span<char> data);
int dump_dbreg(const context& ctx, std::span<char> data);
int dump_mpd(const context& ctx, std::span<char> data);
int dump_otm(const context& ctx, std::span<char> data);
int dump_ufb(const context& ctx, std::span<char> data);
int dump_ucb(const context& ctx, std::span<char> data);
int dump_fms(const context& ctx, std::span<char> data);

} // namespace dump
