#include "audio_banks.hpp"

#include <criware/hca.hpp>
#include <criware/nintendo_opus.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>
#include <utility>

namespace viewer {
namespace {

constexpr uint64_t cache_key(AudioBanks::Group group, uint32_t cue_id) {
	return (static_cast<uint64_t>(group) << 56) | cue_id;
}

uint32_t read_le32(const uint8_t* p) {
	return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
		   (static_cast<uint32_t>(p[3]) << 24);
}

} // namespace

AudioBanks::AudioBanks(const cpk::archive& archive)
	: _archive(archive) {
	_bgm.push_back(Bank{"bgm.acb"});
	_se.push_back(Bank{"se.acb"});
	_voice.push_back(Bank{"voice_jp.acb"});
	_voice.push_back(Bank{"voice_btl_jp.acb"});
	_voice.push_back(Bank{"voice_add_jp.acb"});
}

AudioBanks::~AudioBanks() = default;

bool AudioBanks::load_bank(Bank& bank) {
	bank.loaded = true;
	const auto entry = _archive.find("Sound", bank.file);
	if (entry == _archive.end()) {
		spdlog::debug("audio: bank not found: {}", bank.file);
		return false;
	}

	std::vector<char> bytes;
	try {
		_archive.extract(*entry, bytes);
	} catch (const std::exception& e) {
		spdlog::warn("audio: cannot read {}: {}", bank.file, e.what());
		return false;
	}

	try {
		bank.acb.emplace(criware::ACB::parse(
			std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size())));
	} catch (const std::exception& e) {
		spdlog::warn("audio: cannot parse {}: {}", bank.file, e.what());
		return false;
	}

	for (size_t port = 0; port < bank.acb->stream_awb_names().size(); ++port) {
		const auto& name = bank.acb->stream_awb_name(static_cast<uint16_t>(port));
		if (name.empty())
			continue;
		const auto awb = _archive.find("Sound", name + ".awb");
		if (awb == _archive.end()) {
			spdlog::debug("audio: external AWB not found: {}.awb", name);
			continue;
		}

		std::vector<uint8_t> head;
		try {
			_archive.extract_range(*awb, 0, 16, head);
		} catch (const std::exception&) {
			continue;
		}
		if (head.size() < 16 || std::memcmp(head.data(), "AFS2", 4) != 0)
			continue;
		const uint32_t version = read_le32(head.data() + 4);
		const uint32_t count = read_le32(head.data() + 8);
		const uint32_t stride = (version >> 8) & 0xFF;
		if (stride < 2 || stride > 8)
			continue;
		const uint64_t header_size = 0x10ull + static_cast<uint64_t>(count) * stride * 2;
		const uint64_t archive_size = awb->extract_size != 0 ? awb->extract_size : awb->file_size;
		if (header_size > archive_size)
			continue;

		std::vector<uint8_t> header;
		try {
			_archive.extract_range(*awb, 0, header_size, header);
		} catch (const std::exception&) {
			continue;
		}
		bank.awb_headers.push_back(std::move(header));
		bank.acb->bind_external_awb(
			static_cast<uint16_t>(port),
			std::span<const uint8_t>(bank.awb_headers.back().data(), bank.awb_headers.back().size()),
			archive_size);
	}

	spdlog::debug(
		"audio: loaded {}: {} cues, {} waveforms", bank.file, bank.acb->cues().size(), bank.acb->waveforms().size());
	return true;
}

void AudioBanks::ensure_loaded(Group group) {
	auto& banks = group == Group::Bgm ? _bgm : group == Group::Se ? _se : _voice;
	for (auto& bank : banks) {
		if (!bank.loaded)
			load_bank(bank);
	}
}

std::optional<std::vector<float>>
AudioBanks::decode_waveform(const criware::ACB& acb, const criware::ACBWaveform& waveform) {
	const auto location = acb.locate_waveform(waveform);
	if (!location)
		return std::nullopt;

	if (location->internal) {
		const auto raw = acb.raw_data();
		if (location->offset > raw.size() || location->size > raw.size() - location->offset)
			return std::nullopt;
		const auto data = raw.subspan(static_cast<size_t>(location->offset), static_cast<size_t>(location->size));
		if (waveform.encode_type == 2) {
			criware::HcaDecoder decoder(data);
			if (!decoder.valid())
				return std::nullopt;
			return decoder.decode_all(data);
		}
		if (waveform.encode_type == 24)
			return criware::NintendoOpusDecoder::decode(data, static_cast<int32_t>(waveform.num_samples));
		return std::nullopt;
	}

	const auto& name = acb.stream_awb_name(location->port);
	if (name.empty())
		return std::nullopt;
	const auto entry = _archive.find("Sound", name + ".awb");
	if (entry == _archive.end())
		return std::nullopt;

	std::vector<uint8_t> data;
	try {
		_archive.extract_range(*entry, location->offset, location->size, data);
	} catch (const std::exception&) {
		return std::nullopt;
	}
	if (waveform.encode_type == 2) {
		criware::HcaDecoder decoder(data);
		if (!decoder.valid())
			return std::nullopt;
		return decoder.decode_all(data);
	}
	if (waveform.encode_type == 24)
		return criware::NintendoOpusDecoder::decode(data, static_cast<int32_t>(waveform.num_samples));
	return std::nullopt;
}

std::shared_ptr<const DecodedAudio> AudioBanks::decode_cue(Bank& bank, const criware::ACBCue& cue) {
	std::vector<std::vector<float>> parts;
	uint32_t channels = 1;
	uint32_t sample_rate = 0;
	size_t frames = 0;

	for (uint16_t index : cue.waveform_indices) {
		const auto* waveform = bank.acb->waveform_at(index);
		if (!waveform)
			continue;
		auto samples = decode_waveform(*bank.acb, *waveform);
		if (!samples || samples->empty())
			continue;
		const uint32_t source_channels = waveform->num_channels != 0 ? waveform->num_channels : 1;
		const uint32_t source_rate = waveform->sampling_rate != 0 ? waveform->sampling_rate : 48000;
		if (parts.empty()) {
			channels = source_channels;
			sample_rate = source_rate;
		} else if (source_channels != channels || source_rate != sample_rate) {
			continue;
		}
		frames = std::max(frames, samples->size() / channels);
		parts.push_back(std::move(*samples));
	}

	if (parts.empty() || frames == 0)
		return nullptr;

	auto decoded = std::make_shared<DecodedAudio>();
	decoded->samples.assign(frames * 2, 0.0f);
	for (const auto& part : parts) {
		const size_t count = part.size() / channels;
		for (size_t i = 0; i < count; ++i) {
			const float left = part[i * channels];
			const float right = channels > 1 ? part[i * channels + 1] : left;
			decoded->samples[i * 2] += left;
			decoded->samples[i * 2 + 1] += right;
		}
	}
	for (float& sample : decoded->samples)
		sample = std::clamp(sample, -1.0f, 1.0f);

	decoded->cue_id = cue.cue_id;
	decoded->sample_rate = sample_rate;
	decoded->num_frames = static_cast<uint32_t>(frames);
	decoded->loop = cue.loop;
	if (decoded->loop) {
		decoded->loop_start = cue.loop_start;
		decoded->loop_end = cue.loop_end != 0 ? cue.loop_end : static_cast<uint32_t>(frames);
		if (decoded->loop_start >= decoded->loop_end || decoded->loop_end > frames) {
			decoded->loop_start = 0;
			decoded->loop_end = static_cast<uint32_t>(frames);
		}
	}
	return decoded;
}

void AudioBanks::touch(uint64_t key) {
	const auto it = _cache.find(key);
	if (it == _cache.end())
		return;
	_lru.erase(it->second.lru);
	_lru.push_front(key);
	it->second.lru = _lru.begin();
}

void AudioBanks::insert_cache(uint64_t key, std::shared_ptr<const DecodedAudio> audio) {
	if (!audio)
		return;
	const size_t bytes = audio->samples.size() * sizeof(float);
	while (!_lru.empty() && _cache_bytes + bytes > _cache_limit) {
		const uint64_t victim = _lru.back();
		_lru.pop_back();
		const auto it = _cache.find(victim);
		if (it != _cache.end()) {
			_cache_bytes -= it->second.audio->samples.size() * sizeof(float);
			_cache.erase(it);
		}
	}
	_lru.push_front(key);
	_cache_bytes += bytes;
	_cache.emplace(key, CacheEntry{std::move(audio), _lru.begin()});
}

std::shared_ptr<const DecodedAudio> AudioBanks::load(Group group, uint32_t cue_id) {
	const uint64_t key = cache_key(group, cue_id);
	const auto cached = _cache.find(key);
	if (cached != _cache.end()) {
		touch(key);
		return cached->second.audio;
	}

	ensure_loaded(group);
	auto& banks = group == Group::Bgm ? _bgm : group == Group::Se ? _se : _voice;
	for (auto& bank : banks) {
		if (!bank.acb)
			continue;
		const auto* cue = bank.acb->find_cue(cue_id);
		if (!cue)
			continue;
		auto decoded = decode_cue(bank, *cue);
		if (!decoded)
			continue;
		insert_cache(key, decoded);
		return decoded;
	}
	return nullptr;
}

std::shared_ptr<const DecodedAudio> AudioBanks::load_by_name(Group group, std::string_view name) {
	ensure_loaded(group);
	auto& banks = group == Group::Bgm ? _bgm : group == Group::Se ? _se : _voice;
	for (auto& bank : banks) {
		if (!bank.acb)
			continue;
		for (const auto& cue : bank.acb->cues()) {
			if (cue.cue_name != name)
				continue;
			return load(group, cue.cue_id);
		}
	}
	return nullptr;
}

int AudioBanks::cue_frames(const criware::ACB& acb, const criware::ACBCue& cue) const {
	uint32_t sample_rate = 0;
	if (!cue.waveform_indices.empty()) {
		if (const auto* waveform = acb.waveform_at(cue.waveform_indices.front()))
			sample_rate = waveform->sampling_rate;
	}
	if (sample_rate == 0)
		sample_rate = 48000;
	const uint32_t samples = cue.length != 0 ? cue.length : 0;
	if (samples == 0)
		return 0;
	return static_cast<int>(std::lround(static_cast<double>(samples) * 60.0 / static_cast<double>(sample_rate)));
}

int AudioBanks::cue_length_frames(uint32_t cue_id) {
	auto& bgm = _bgm;
	auto& se = _se;
	auto& voice = _voice;
	auto lookup = [&](auto& banks) -> int {
		for (auto& bank : banks) {
			if (!bank.acb)
				continue;
			if (const auto* cue = bank.acb->find_cue(cue_id))
				return cue_frames(*bank.acb, *cue);
		}
		return -1;
	};

	if (const int frames = lookup(bgm); frames >= 0)
		return frames;
	if (const int frames = lookup(se); frames >= 0)
		return frames;
	if (const int frames = lookup(voice); frames >= 0)
		return frames;

	ensure_loaded(Group::Bgm);
	if (const int frames = lookup(bgm); frames >= 0)
		return frames;
	ensure_loaded(Group::Se);
	if (const int frames = lookup(se); frames >= 0)
		return frames;
	ensure_loaded(Group::Voice);
	if (const int frames = lookup(voice); frames >= 0)
		return frames;
	return 0;
}

size_t AudioBanks::loaded_banks() const {
	size_t count = 0;
	for (const auto* banks : {&_bgm, &_se, &_voice})
		for (const auto& bank : *banks)
			count += bank.loaded ? 1 : 0;
	return count;
}

} // namespace viewer
