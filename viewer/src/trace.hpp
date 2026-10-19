#pragma once

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <string>

// Lightweight stage tracing for startup timing: prints "[trace +NNN.Nms] ..."
// relative to the last reset_trace() (called at the top of main()). Per-scope
// detail lines are debug-level and controlled by VIEWER_LOG_LEVEL.
namespace viewer::trace {

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
	char message[1024];
	va_list ap;
	va_start(ap, fmt);
	std::vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	spdlog::info("[trace +{:9.1f}ms] {}", ms_since_origin(), message);
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
		const auto d = std::chrono::steady_clock::now() - _t0;
		spdlog::debug(
			"[trace +{:9.1f}ms]   {}: {:.1f}ms",
			ms_since_origin(),
			_label,
			std::chrono::duration<double, std::milli>(d).count());
	}

private:
	std::string _label;
	std::chrono::steady_clock::time_point _t0;
};

} // namespace viewer::trace
