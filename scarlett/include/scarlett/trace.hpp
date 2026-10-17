#pragma once

#include <cstdio>
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

inline std::string number(double value) {
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%.7g", value);
	return buf;
}

inline std::string integer(long long value) {
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%lld", value);
	return buf;
}

inline std::string boolean(bool value) { return value ? "true" : "false"; }

inline std::string vec2(const float* v) { return "[" + number(v[0]) + "," + number(v[1]) + "]"; }

inline std::string vec3(const float* v) { return "[" + number(v[0]) + "," + number(v[1]) + "," + number(v[2]) + "]"; }

inline std::string vec4(const float* v) {
	return "[" + number(v[0]) + "," + number(v[1]) + "," + number(v[2]) + "," + number(v[3]) + "]";
}

} // namespace trace
} // namespace asb
