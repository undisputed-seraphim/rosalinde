#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace criware {

struct HcaInfo {
	uint32_t version = 0;
	uint32_t header_size = 0;
	uint32_t sample_rate = 0;
	uint32_t channels = 0;
	uint32_t block_size = 0;
	uint32_t block_count = 0;
	uint32_t samples_per_block = 1024;
	uint32_t encoder_delay = 0;
	uint32_t encoder_padding = 0;
	uint32_t sample_count = 0;
	bool loop_enabled = false;
	uint32_t loop_start_block = 0;
	uint32_t loop_end_block = 0;
	uint32_t loop_start_sample = 0;
	uint32_t loop_end_sample = 0;
	uint16_t cipher_type = 0;
	float rva_volume = 1.0f;
};

class HcaDecoder {
public:
	HcaDecoder() = default;
	explicit HcaDecoder(std::span<const uint8_t> data, uint64_t key = 0);
	~HcaDecoder();

	HcaDecoder(const HcaDecoder&) = delete;
	HcaDecoder& operator=(const HcaDecoder&) = delete;
	HcaDecoder(HcaDecoder&&) noexcept;
	HcaDecoder& operator=(HcaDecoder&&) noexcept;

	static std::optional<HcaInfo> parse(std::span<const uint8_t> data, uint64_t key = 0);

	bool valid() const;
	const HcaInfo& info() const;

	size_t decode_block(std::span<const uint8_t> block, float* out);
	std::vector<float> decode_all(std::span<const uint8_t> data);

private:
	struct Impl;
	std::unique_ptr<Impl> _impl;
};

} // namespace criware
