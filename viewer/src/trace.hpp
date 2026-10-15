#pragma once

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>

// Lightweight stage tracing for startup timing: prints "[trace +NNN.Nms] ..."
// relative to the last reset_trace() (called at the top of main()). Per-scope
// detail lines (Scoped) are only printed when $VIEWER_TRACE_DETAIL is set.
namespace viewer::trace {

inline bool detail_enabled() {
	static const bool enabled = std::getenv("VIEWER_TRACE_DETAIL") != nullptr;
	return enabled;
}

inline std::chrono::steady_clock::time_point& origin() {
	static std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();
	return t;
}

inline void reset() { origin() = std::chrono::steady_clock::now(); }

inline double ms_since_origin() {
	const auto d = std::chrono::steady_clock::now() - origin();
	return std::chrono::duration<double, std::milli>(d).count();
}

inline void stamp(const char* fmt, ...) {
	std::fprintf(stderr, "[trace +%9.1fms] ", ms_since_origin());
	va_list ap;
	va_start(ap, fmt);
	std::vfprintf(stderr, fmt, ap);
	va_end(ap);
	std::fprintf(stderr, "\n");
}

// Prints the elapsed time of its scope, e.g. "[trace +  123.4ms] label: 12.3ms".
class Scoped {
public:
	explicit Scoped(std::string label)
		: _label(std::move(label))
		, _t0(std::chrono::steady_clock::now()) {}

	Scoped(const Scoped&) = delete;
	Scoped& operator=(const Scoped&) = delete;

	~Scoped() {
		if (!detail_enabled())
			return;
		const auto d = std::chrono::steady_clock::now() - _t0;
		std::fprintf(
			stderr,
			"[trace +%9.1fms]   %s: %.1fms\n",
			ms_since_origin(),
			_label.c_str(),
			std::chrono::duration<double, std::milli>(d).count());
	}

private:
	std::string _label;
	std::chrono::steady_clock::time_point _t0;
};

} // namespace viewer::trace
