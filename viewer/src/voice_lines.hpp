#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace viewer {

class VoiceLines {
public:
	bool load(const std::filesystem::path& path);
	std::optional<uint32_t> cue_for_text(uint32_t text_id) const;
	size_t size() const { return _rows.size(); }

private:
	struct Row {
		uint32_t text_id = 0;
		uint32_t cue_id = 0;
	};

	std::vector<Row> _rows;
};

} // namespace viewer
