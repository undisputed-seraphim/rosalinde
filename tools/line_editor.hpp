#pragma once

#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace cli {

class line_editor {
public:
	line_editor();
	~line_editor();

	line_editor(const line_editor&) = delete;
	line_editor& operator=(const line_editor&) = delete;

	std::optional<std::string> read_line(std::string_view prompt, const std::deque<std::string>& history);

private:
	struct impl;
	std::unique_ptr<impl> _impl;
};

} // namespace cli
