#pragma once

#include <criware/acb.hpp>
#include <criware/cpk.hpp>

#include <cstdint>
#include <deque>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace viewer {

struct DecodedAudio {
	std::vector<float> samples;
	uint32_t sample_rate = 48000;
	uint32_t num_frames = 0;
	bool loop = false;
	uint32_t loop_start = 0;
	uint32_t loop_end = 0;
	uint32_t cue_id = 0;
};

class AudioBanks {
public:
	enum class Group {
		Bgm,
		Se,
		Voice,
	};

	explicit AudioBanks(const cpk::archive& archive);
	~AudioBanks();

	AudioBanks(const AudioBanks&) = delete;
	AudioBanks& operator=(const AudioBanks&) = delete;

	std::shared_ptr<const DecodedAudio> load(Group group, uint32_t cue_id);
	std::shared_ptr<const DecodedAudio> load_by_name(Group group, std::string_view name);
	int cue_length_frames(uint32_t cue_id);

	size_t loaded_banks() const;
	size_t cache_bytes() const { return _cache_bytes; }

private:
	struct Bank {
		std::string file;
		std::optional<criware::ACB> acb;
		std::deque<std::vector<uint8_t>> awb_headers;
		bool loaded = false;
	};

	struct CacheEntry {
		std::shared_ptr<const DecodedAudio> audio;
		std::list<uint64_t>::iterator lru;
	};

	void ensure_loaded(Group group);
	bool load_bank(Bank& bank);
	std::shared_ptr<const DecodedAudio> decode_bank(Bank& bank, uint32_t cue_id);
	std::shared_ptr<const DecodedAudio> decode_cue(Bank& bank, const criware::ACBCue& cue);
	std::optional<std::vector<float>> decode_waveform(const criware::ACB& acb, const criware::ACBWaveform& waveform);
	int cue_frames(const criware::ACB& acb, const criware::ACBCue& cue) const;

	void touch(uint64_t key);
	void insert_cache(uint64_t key, std::shared_ptr<const DecodedAudio> audio);

	const cpk::archive& _archive;
	std::vector<Bank> _bgm;
	std::vector<Bank> _se;
	std::vector<Bank> _voice;
	std::list<uint64_t> _lru;
	std::unordered_map<uint64_t, CacheEntry> _cache;
	size_t _cache_bytes = 0;
	size_t _cache_limit = 96ull * 1024ull * 1024ull;
};

} // namespace viewer
