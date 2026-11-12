#pragma once

#include "audio_banks.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct SDL_AudioStream;

namespace asb {
struct RuntimeAudio;
}

namespace viewer {

class AudioEngine {
public:
	struct Options {
		bool realtime = false;
		bool offline = false;
		std::string wav_path;
		uint32_t sample_rate = 48000;
		float master_volume = 1.0f;
	};

	AudioEngine(std::shared_ptr<AudioBanks> banks, Options options);
	~AudioEngine();

	AudioEngine(const AudioEngine&) = delete;
	AudioEngine& operator=(const AudioEngine&) = delete;

	bool active() const { return _active; }
	bool realtime() const { return _stream != nullptr; }
	bool offline() const { return _offline; }
	float sample_rate() const { return static_cast<float>(_sample_rate); }

	void handle(const asb::RuntimeAudio& event);
	void tick(long frame);
	void stop_all();

	std::string state_json() const;
	std::string summary() const;

private:
	enum class Bus {
		Bgm = 0,
		Se = 1,
		Voice = 2,
		Ambience = 3,
		Count = 4,
	};

	struct Voice {
		std::shared_ptr<const DecodedAudio> audio;
		double position = 0.0;
		uint32_t cue = 0;
		uint8_t bus = 0;
		float envelope_from = 1.0f;
		float envelope_to = 1.0f;
		float envelope = 1.0f;
		uint64_t envelope_length = 0;
		uint64_t envelope_position = 0;
		bool stop_at_end = false;
	};

	struct BusState {
		float gain_from = 1.0f;
		float gain_to = 1.0f;
		float gain = 1.0f;
		uint64_t fade_length = 0;
		uint64_t fade_position = 0;
	};

	static void sdl_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);

	static constexpr size_t bus_count = 4;
	static float current_envelope(const Voice& voice);
	static float advance_envelope(Voice& voice);
	static float advance_bus(BusState& bus);

	void mix(float* out, int frames);
	bool play(Bus bus, AudioBanks::Group group, uint32_t cue, int fade_frames, uint32_t start_ms = 0);
	bool play_by_name(Bus bus, AudioBanks::Group group, const std::string& name, int fade_frames);
	void stop_bus(Bus bus, int fade_frames);
	void stop_bus_locked(Bus bus, int fade_frames);
	void stop_cue(Bus bus, uint32_t cue, int fade_frames);
	void set_bus_volume(Bus bus, float volume, int fade_frames);
	void begin_stop(Voice& voice, int fade_frames);
	bool open_device();
	void write_wav(const float* samples, size_t frames);

	std::shared_ptr<AudioBanks> _banks;
	Options _options;
	bool _active = false;
	bool _offline = false;
	uint32_t _sample_rate = 48000;
	SDL_AudioStream* _stream = nullptr;
	bool _sdl_audio_started = false;
	size_t _rendered_frames = 0;
	long _last_frame = 0;
	uint64_t _events = 0;
	uint32_t _stage_bgm = 0;

	mutable std::mutex _mutex;
	std::vector<Voice> _voices;
	BusState _buses[static_cast<size_t>(Bus::Count)];
	float _master = 1.0f;
	std::ofstream _wav_file;
	uint64_t _wav_data_bytes = 0;
};

} // namespace viewer
