#include "voice_lines.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

namespace viewer {

bool VoiceLines::load(const std::filesystem::path& path) {
	std::ifstream file(path);
	if (!file)
		return false;

	std::string line;
	std::getline(file, line);
	while (std::getline(file, line)) {
		if (line.empty() || line[0] == '#')
			continue;
		std::istringstream row(line);
		std::string text;
		std::string cue;
		if (!std::getline(row, text, '\t') || !std::getline(row, cue, '\t'))
			continue;
		try {
			_rows.push_back({
				static_cast<uint32_t>(std::stoul(text)),
				static_cast<uint32_t>(std::stoul(cue)),
			});
		} catch (const std::exception&) {
		}
	}

	std::stable_sort(_rows.begin(), _rows.end(), [](const Row& a, const Row& b) {
		return a.text_id < b.text_id;
	});

	spdlog::debug("audio: loaded {} raw voice-line rows from {}", _rows.size(), path.string());
	return !_rows.empty();
}

std::optional<uint32_t> VoiceLines::cue_for_text(uint32_t text_id) const {
	const auto it = std::lower_bound(
		_rows.begin(),
		_rows.end(),
		text_id,
		[](const Row& row, uint32_t value) { return row.text_id < value; });
	if (it == _rows.end() || it->text_id != text_id)
		return std::nullopt;
	return it->cue_id;
}

} // namespace viewer
