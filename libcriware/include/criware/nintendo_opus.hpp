#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace criware {

struct NintendoOpusInfo {
	uint8_t channels = 0;
	uint16_t skip = 0;
	uint32_t sample_rate = 48000;
	uint32_t data_offset = 0;
	uint32_t data_size = 0;
	uint32_t num_samples = 0;
	bool loop_enabled = false;
	uint32_t loop_start = 0;
	uint32_t loop_end = 0;
};

class NintendoOpusDecoder {
public:
	static std::optional<NintendoOpusInfo> parse(std::span<const uint8_t> data);
	static std::vector<float> decode(std::span<const uint8_t> data, int32_t expected_samples = -1);
};

} // namespace criware
