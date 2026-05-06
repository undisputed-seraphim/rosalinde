#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <criware/afs2.hpp>
#include <criware/utf.hpp>

namespace criware {

struct ACBWaveform {
	uint8_t encode_type = 0;
	uint8_t streaming = 0;
	uint8_t loop_flag = 0;
	uint8_t num_channels = 0;
	uint32_t sampling_rate = 0;
	uint32_t num_samples = 0;
	uint16_t memory_awb_id = 0;
	uint16_t stream_awb_id = 0;
	uint16_t waveform_id = 0;
};

struct ACBCue {
	uint32_t cue_id = 0;
	uint8_t reference_type = 0;
	uint16_t reference_index = 0;

	uint16_t waveform_index = 0;
	uint16_t waveform_id = 0;
	uint8_t encode_type = 0;
	bool streaming = false;
	bool waveform_identified = false;

	std::string cue_name;
};

struct ACBTrack {
	uint32_t track_index = 0;
	uint16_t synth_index = 0;
	uint16_t waveform_index = 0;
	uint16_t waveform_id = 0;
	uint8_t encode_type = 0;
	bool streaming = false;
	bool waveform_identified = false;
};

class ACB {
public:
	ACB() = default;
	ACB(const ACB&) = delete;
	ACB(ACB&&) = default;
	ACB& operator=(ACB&&) = default;

	static ACB parse(std::span<const uint8_t> data);

	const std::vector<ACBCue>& cues() const { return _cues; }
	const std::vector<ACBTrack>& tracks() const { return _tracks; }
	const std::vector<ACBWaveform>& waveforms() const { return _waveforms; }

	const ACBCue* find_cue(uint32_t cue_id) const;
	const ACBTrack* find_track(uint32_t track_index) const;
	const ACBWaveform* find_waveform(uint16_t waveform_id) const;

	uint32_t format_version() const { return _format_version; }

	bool extract_waveform(uint16_t waveform_id, std::vector<char>& out) const;
	std::span<const uint8_t> waveform_span(uint16_t waveform_id) const;
	const AFS2* internal_awb() const;

private:
	std::vector<ACBCue> _cues;
	std::vector<ACBTrack> _tracks;
	std::vector<ACBWaveform> _waveforms;
	uint32_t _format_version = 0;

	std::span<const uint8_t> _raw_data;
	UTF::field::data_t _awb_file{};
	mutable std::optional<AFS2> _internal_awb_cache;
};

} // namespace criware
