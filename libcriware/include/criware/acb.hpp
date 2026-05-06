#pragma once

#include <cstdint>
#include <string>
#include <vector>

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

	uint32_t format_version() const { return _format_version; }

private:
	std::vector<ACBCue> _cues;
	std::vector<ACBTrack> _tracks;
	std::vector<ACBWaveform> _waveforms;
	uint32_t _format_version = 0;
};

} // namespace criware
