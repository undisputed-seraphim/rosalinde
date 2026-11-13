#include "log.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

spdlog::level::level_enum configured_level() {
	std::string_view name;
	if (const char* value = std::getenv("VIEWER_LOG_LEVEL"))
		name = value;
	else
		name = "info";

	if (name == "trace")
		return spdlog::level::trace;
	if (name == "debug")
		return spdlog::level::debug;
	if (name == "info")
		return spdlog::level::info;
	if (name == "warn" || name == "warning")
		return spdlog::level::warn;
	if (name == "error" || name == "err")
		return spdlog::level::err;
	if (name == "critical" || name == "fatal")
		return spdlog::level::critical;
	if (name == "off")
		return spdlog::level::off;
	return spdlog::level::info;
}

} // namespace

void viewer::log::init() {
	if (spdlog::get("viewer"))
		return;
#ifdef _WIN32
	// Windows consoles interpret raw bytes with their output code page; the
	// LANG/LC_ALL environment does not change it. Force UTF-8 so UTF-8 log
	// messages and direct stderr writes are not garbled. spdlog's stderr sink
	// additionally writes wide characters when built with
	// SPDLOG_UTF8_TO_WCHAR_CONSOLE (see thirdparty/spdlog.cmake).
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
#endif
	auto logger = spdlog::stderr_color_mt("viewer");
	spdlog::set_default_logger(std::move(logger));
	spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
	spdlog::set_level(configured_level());
	spdlog::flush_on(spdlog::level::info);
}

void viewer::log::shutdown() { spdlog::shutdown(); }
