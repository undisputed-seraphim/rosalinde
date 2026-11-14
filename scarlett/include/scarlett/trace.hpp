#pragma once

#include <cstdio>
#include <format>
#include <functional>
#include <string>
#include <string_view>

namespace asb {

struct TraceEvent {
	long frame = 0;
	std::string_view kind;
	std::string_view event;
	std::string_view fields;
};

using TraceCallback = std::function<void(const TraceEvent&)>;

// One complete native invocation. Unlike TraceEvent, this owns its strings so a
// consumer can retain or reorder it.
struct NativeCall {
	long frame = 0;
	std::string name;
	std::string call;
	std::string caller;
	bool handled = false;
	std::string return_type;
	std::string result;
};

using NativeCallCallback = std::function<void(const NativeCall&)>;

namespace trace {

inline std::string quote(std::string_view value) {
	std::string out;
	out.reserve(value.size() + 8);
	for (unsigned char c : value) {
		switch (c) {
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if (c < 0x20) {
				char buf[8];
				std::snprintf(buf, sizeof(buf), "\\u%04x", c);
				out += buf;
			} else {
				out += static_cast<char>(c);
			}
		}
	}
	return out;
}

inline std::string number(double value) { return std::format("{:.7g}", value); }

inline std::string integer(long long value) { return std::format("{}", value); }

inline std::string boolean(bool value) { return value ? "true" : "false"; }

inline std::string vec2(const float* v) { return std::format("[{:.7g},{:.7g}]", v[0], v[1]); }

inline std::string vec3(const float* v) { return std::format("[{:.7g},{:.7g},{:.7g}]", v[0], v[1], v[2]); }

inline std::string vec4(const float* v) { return std::format("[{:.7g},{:.7g},{:.7g},{:.7g}]", v[0], v[1], v[2], v[3]); }

} // namespace trace
} // namespace asb
