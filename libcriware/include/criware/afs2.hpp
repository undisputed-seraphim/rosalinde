#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace criware {

class AFS2 {
public:
	struct Entry {
		uint16_t id = 0;        // waveform ID (always uint16 LE)
		uint64_t offset = 0;    // raw offset from stream (before alignment add)
		uint64_t aligned = 0;   // offset rounded up to alignment boundary
		uint64_t size = 0;      // file data size in bytes
	};

	AFS2() = default;
	AFS2(const AFS2&) = default;
	AFS2(AFS2&&) = default;
	AFS2& operator=(const AFS2&) = default;
	AFS2& operator=(AFS2&&) = default;

	static AFS2 parse(std::span<const uint8_t> data, uint64_t base_offset = 0);

	const Entry* find(uint16_t id) const;
	const std::vector<Entry>& entries() const { return _entries; }

	uint32_t version() const { return _version; }
	uint32_t alignment() const { return _alignment; }
	uint16_t hca_key_modifier() const { return _hca_key; }

private:
	std::vector<Entry> _entries;
	uint32_t _version = 0;
	uint32_t _alignment = 0;
	uint16_t _hca_key = 0;
};

} // namespace criware
