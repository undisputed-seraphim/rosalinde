#include "audio_engine.hpp"

#include <scarlett/scene.hpp>

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <sstream>

namespace viewer {
namespace {

void write_le16(std::ostream& out, uint16_t value) {
	const uint8_t bytes[2] = {
		static_cast<uint8_t>(value & 0xFF),
		static_cast<uint8_t>((value >> 8) & 0xFF),
	};
	out.write(reinterpret_cast<const char*>(bytes), 2);
}

void write_le32(std::ostream& out, uint32_t value) {
	const uint8_t bytes[4] = {
		static_cast<uint8_t>(value & 0xFF),
		static_cast<uint8_t>((value >> 8) & 0xFF),
		static_cast<uint8_t>((value >> 16) & 0xFF),
		static_cast<uint8_t>((value >> 24) & 0xFF),
	};
	out.write(reinterpret_cast<const char*>(bytes), 4);
}

std::string quoted(const std::string& text) {
	std::string out = "\"";
	for (char c : text) {
		if (c == '"' || c == '\\')
			out += '\\';
		out += c;
	}
	out += '"';
	return out;
}

} // namespace

float AudioEngine::current_envelope(const Voice& voice) {
	if (voice.envelope_length == 0)
		return voice.envelope_to;
	if (voice.envelope_position >= voice.envelope_length)
		return voice.envelope_to;
	const double t = static_cast<double>(voice.envelope_position) / static_cast<double>(voice.envelope_length);
	return static_cast<float>(voice.envelope_from + (voice.envelope_to - voice.envelope_from) * static_cast<float>(t));
}

float AudioEngine::advance_envelope(Voice& voice) {
	if (voice.envelope_length == 0)
		return voice.envelope = voice.envelope_to;
	if (voice.envelope_position < voice.envelope_length)
		++voice.envelope_position;
	const double t = static_cast<double>(voice.envelope_position) / static_cast<double>(voice.envelope_length);
	return voice.envelope = static_cast<float>(
			   voice.envelope_from + (voice.envelope_to - voice.envelope_from) * static_cast<float>(t));
}

float AudioEngine::advance_bus(BusState& bus) {
	if (bus.fade_length == 0)
		return bus.gain = bus.gain_to;
	if (bus.fade_position < bus.fade_length)
		++bus.fade_position;
	const double t = static_cast<double>(bus.fade_position) / static_cast<double>(bus.fade_length);
	return bus.gain = static_cast<float>(bus.gain_from + (bus.gain_to - bus.gain_from) * static_cast<float>(t));
}

AudioEngine::AudioEngine(std::shared_ptr<AudioBanks> banks, Options options)
	: _banks(std::move(banks))
	, _options(std::move(options))
	, _offline(_options.offline)
	, _sample_rate(_options.sample_rate == 0 ? 48000 : _options.sample_rate)
	, _master(std::clamp(_options.master_volume, 0.0f, 1.0f)) {
	_active = _options.realtime || _offline;

	if (_offline && !_options.wav_path.empty()) {
		const auto parent = std::filesystem::path(_options.wav_path).parent_path();
		if (!parent.empty())
			std::filesystem::create_directories(parent);
		_wav_file.open(_options.wav_path, std::ios::binary | std::ios::trunc);
		if (!_wav_file) {
			spdlog::warn("audio: cannot write {}", _options.wav_path);
		} else {
			_wav_file.write("RIFF", 4);
			write_le32(_wav_file, 0);
			_wav_file.write("WAVEfmt ", 8);
			write_le32(_wav_file, 16);
			write_le16(_wav_file, 1);
			write_le16(_wav_file, 2);
			write_le32(_wav_file, _sample_rate);
			write_le32(_wav_file, _sample_rate * 4);
			write_le16(_wav_file, 4);
			write_le16(_wav_file, 16);
			_wav_file.write("data", 4);
			write_le32(_wav_file, 0);
		}
	}

	if (_options.realtime && !open_device())
		_active = _offline;

	if (!_active)
		spdlog::debug("audio: disabled");
	else if (realtime())
		spdlog::info("audio: realtime {} Hz", _sample_rate);
	else if (!_options.wav_path.empty())
		spdlog::info("audio: offline {} Hz -> {}", _sample_rate, _options.wav_path);
	else
		spdlog::debug("audio: offline {} Hz (no output)", _sample_rate);
}

AudioEngine::~AudioEngine() {
	if (_stream) {
		SDL_DestroyAudioStream(_stream);
		_stream = nullptr;
	}
	if (_wav_file) {
		const auto end = _wav_file.tellp();
		_wav_file.seekp(4);
		write_le32(_wav_file, static_cast<uint32_t>(36 + _wav_data_bytes));
		_wav_file.seekp(40);
		write_le32(_wav_file, static_cast<uint32_t>(_wav_data_bytes));
		_wav_file.seekp(end);
		_wav_file.close();
	}
	if (_sdl_audio_started)
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool AudioEngine::open_device() {
	if (!SDL_WasInit(SDL_INIT_AUDIO)) {
		if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
			spdlog::debug("audio: SDL audio unavailable: {}", SDL_GetError());
			return false;
		}
		_sdl_audio_started = true;
	}

	SDL_AudioSpec spec{};
	spec.format = SDL_AUDIO_F32;
	spec.channels = 2;
	spec.freq = static_cast<int>(_sample_rate);
	_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &AudioEngine::sdl_callback, this);
	if (!_stream) {
		spdlog::debug("audio: cannot open playback stream: {}", SDL_GetError());
		return false;
	}
	SDL_ResumeAudioStreamDevice(_stream);
	return true;
}

void AudioEngine::sdl_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount) {
	(void)total_amount;
	auto* self = static_cast<AudioEngine*>(userdata);
	if (!self || additional_amount <= 0)
		return;
	const int frames = additional_amount / static_cast<int>(sizeof(float) * 2);
	if (frames <= 0)
		return;
	std::vector<float> buffer(static_cast<size_t>(frames) * 2);
	self->mix(buffer.data(), frames);
	SDL_PutAudioStreamData(stream, buffer.data(), static_cast<int>(buffer.size() * sizeof(float)));
}

void AudioEngine::begin_stop(Voice& voice, int fade_frames) {
	voice.envelope_from = current_envelope(voice);
	voice.envelope_to = 0.0f;
	voice.envelope = voice.envelope_from;
	voice.envelope_position = 0;
	voice.envelope_length =
		fade_frames > 0 ? static_cast<uint64_t>(std::lround(static_cast<double>(fade_frames) * _sample_rate / 60.0))
						: 0;
	voice.stop_at_end = true;
	if (voice.envelope_length == 0)
		voice.envelope = 0.0f;
}

bool AudioEngine::play(Bus bus, AudioBanks::Group group, uint32_t cue, int fade_frames, uint32_t start_ms) {
	if (!_banks || cue == 0)
		return false;
	auto audio = _banks->load(group, cue);
	if (!audio) {
		spdlog::debug("audio: cue {} not available in bank {}", cue, static_cast<int>(group));
		return false;
	}

	std::lock_guard lock(_mutex);
	if (bus == Bus::Bgm)
		stop_bus_locked(Bus::Bgm, fade_frames);

	Voice voice;
	voice.audio = std::move(audio);
	voice.bus = static_cast<uint8_t>(bus);
	voice.cue = cue;
	voice.position = static_cast<double>(start_ms) * voice.audio->sample_rate / 1000.0;
	if (voice.position >= voice.audio->num_frames)
		voice.position = 0.0;
	voice.envelope_from = 0.0f;
	voice.envelope_to = 1.0f;
	voice.envelope = 0.0f;
	voice.envelope_length =
		fade_frames > 0 ? static_cast<uint64_t>(std::lround(static_cast<double>(fade_frames) * _sample_rate / 60.0))
						: 0;
	voice.envelope_position = 0;
	if (voice.envelope_length == 0)
		voice.envelope = 1.0f;
	_voices.push_back(std::move(voice));

	spdlog::debug(
		"audio: play bus={} cue={} frames={} loop={} fade={} start_ms={}",
		static_cast<int>(bus),
		cue,
		_voices.back().audio->num_frames,
		_voices.back().audio->loop,
		fade_frames,
		start_ms);
	return true;
}

bool AudioEngine::play_by_name(Bus bus, AudioBanks::Group group, const std::string& name, int fade_frames) {
	if (!_banks || name.empty())
		return false;
	auto audio = _banks->load_by_name(group, name);
	if (!audio) {
		spdlog::debug("audio: cue name '{}' not available in bank {}", name, static_cast<int>(group));
		return false;
	}
	const uint32_t cue = audio->cue_id;
	(void)audio;
	return play(bus, group, cue, fade_frames);
}

void AudioEngine::stop_bus(Bus bus, int fade_frames) {
	std::lock_guard lock(_mutex);
	stop_bus_locked(bus, fade_frames);
}

void AudioEngine::stop_bus_locked(Bus bus, int fade_frames) {
	for (auto& voice : _voices) {
		if (voice.bus != static_cast<uint8_t>(bus) || voice.stop_at_end)
			continue;
		begin_stop(voice, fade_frames);
	}
}

void AudioEngine::stop_cue(Bus bus, uint32_t cue, int fade_frames) {
	std::lock_guard lock(_mutex);
	for (auto& voice : _voices) {
		if (voice.bus != static_cast<uint8_t>(bus) || voice.stop_at_end)
			continue;
		if (cue != 0 && voice.cue != cue)
			continue;
		begin_stop(voice, fade_frames);
	}
}

void AudioEngine::set_bus_volume(Bus bus, float volume, int fade_frames) {
	std::lock_guard lock(_mutex);
	auto& state = _buses[static_cast<size_t>(bus)];
	state.gain_from = state.gain;
	state.gain_to = std::clamp(volume, 0.0f, 1.0f);
	state.fade_position = 0;
	state.fade_length = fade_frames > 0
							? static_cast<uint64_t>(std::lround(static_cast<double>(fade_frames) * _sample_rate / 60.0))
							: 0;
	if (state.fade_length == 0)
		state.gain = state.gain_to;
}

void AudioEngine::handle(const asb::RuntimeAudio& event) {
	++_events;
	if (!_active)
		return;
	const int fade = std::max(0, event.fade);
	if (event.kind == "bgm") {
		play(Bus::Bgm, AudioBanks::Group::Bgm, static_cast<uint32_t>(std::max(0, event.cue)), fade);
	} else if (event.kind == "bgm_time") {
		const uint32_t start = event.index > 0 ? static_cast<uint32_t>(event.index) : 0;
		play(Bus::Bgm, AudioBanks::Group::Bgm, static_cast<uint32_t>(std::max(0, event.cue)), fade, start);
	} else if (event.kind == "bgm_default") {
		if (_stage_bgm != 0)
			play(Bus::Bgm, AudioBanks::Group::Bgm, _stage_bgm, fade);
	} else if (event.kind == "stage_bgm") {
		_stage_bgm = static_cast<uint32_t>(std::max(0, event.cue));
	} else if (event.kind == "stop_bgm") {
		stop_bus(Bus::Bgm, fade);
	} else if (event.kind == "bgm_volume") {
		set_bus_volume(Bus::Bgm, event.volume, fade);
	} else if (event.kind == "se") {
		const uint32_t cue = static_cast<uint32_t>(std::max(0, event.cue));
		if (!play(Bus::Se, AudioBanks::Group::Se, cue, 0))
			play(Bus::Voice, AudioBanks::Group::Voice, cue, 0);
	} else if (event.kind == "se_chara") {
		play(Bus::Voice, AudioBanks::Group::Voice, static_cast<uint32_t>(std::max(0, event.cue)), 0);
	} else if (event.kind == "stop_se") {
		const uint32_t cue = static_cast<uint32_t>(std::max(0, event.cue));
		const int stop_fade = event.fade < 0 ? 0 : event.fade;
		stop_cue(Bus::Se, cue, stop_fade);
		stop_cue(Bus::Voice, cue, stop_fade);
	} else if (event.kind == "se_volume") {
		set_bus_volume(Bus::Se, event.volume, fade);
	} else if (event.kind == "amb_volume") {
		set_bus_volume(Bus::Ambience, event.volume, fade);
	} else if (event.kind == "amb_fade") {
		set_bus_volume(Bus::Ambience, event.volume, 0);
	} else if (event.kind == "action_cue") {
		play(Bus::Se, AudioBanks::Group::Se, static_cast<uint32_t>(std::max(0, event.cue)), 0);
	} else if (event.kind == "battle_bgm") {
		if (event.cue > 0)
			play(Bus::Bgm, AudioBanks::Group::Bgm, static_cast<uint32_t>(event.cue), fade);
		else if (!event.target.empty())
			play_by_name(Bus::Bgm, AudioBanks::Group::Bgm, event.target, fade);
	} else {
		spdlog::debug("audio: unhandled event kind '{}'", event.kind);
	}
}

void AudioEngine::mix(float* out, int frames) {
	std::fill(out, out + static_cast<size_t>(frames) * 2, 0.0f);

	std::lock_guard lock(_mutex);
	std::array<std::vector<float>, bus_count> gains;
	for (size_t bus = 0; bus < bus_count; ++bus) {
		gains[bus].resize(static_cast<size_t>(frames));
		auto& state = _buses[bus];
		for (int i = 0; i < frames; ++i)
			gains[bus][static_cast<size_t>(i)] = advance_bus(state);
	}

	for (auto& voice : _voices) {
		const auto& data = *voice.audio;
		const size_t total = data.num_frames;
		if (total == 0)
			continue;
		const double step = static_cast<double>(data.sample_rate) / static_cast<double>(_sample_rate);
		const double loop_start = data.loop_start;
		const double loop_end = data.loop_end != 0 ? data.loop_end : static_cast<double>(total);
		const auto& bus = gains[voice.bus];

		for (int i = 0; i < frames; ++i) {
			if (voice.stop_at_end && (voice.envelope_length == 0 || voice.envelope_position >= voice.envelope_length))
				break;
			if (voice.position >= static_cast<double>(total)) {
				if (data.loop && loop_end > loop_start)
					voice.position = loop_start + std::fmod(voice.position - loop_start, loop_end - loop_start);
				else {
					voice.position = static_cast<double>(total);
					voice.stop_at_end = true;
					voice.envelope = 0.0f;
					break;
				}
			}

			const size_t index = static_cast<size_t>(voice.position);
			const float fraction = static_cast<float>(voice.position - static_cast<double>(index));
			const float left0 = data.samples[index * 2];
			const float right0 = data.samples[index * 2 + 1];
			float left1 = left0;
			float right1 = right0;
			if (index + 1 < total) {
				left1 = data.samples[(index + 1) * 2];
				right1 = data.samples[(index + 1) * 2 + 1];
			}
			const float left = left0 + (left1 - left0) * fraction;
			const float right = right0 + (right1 - right0) * fraction;
			const float envelope = advance_envelope(voice);
			const float gain = envelope * bus[static_cast<size_t>(i)] * _master;
			out[i * 2] += left * gain;
			out[i * 2 + 1] += right * gain;

			voice.position += step;
			if (data.loop && loop_end > loop_start) {
				while (voice.position >= loop_end)
					voice.position -= loop_end - loop_start;
			}
		}
	}

	for (size_t i = 0; i < static_cast<size_t>(frames) * 2; ++i)
		out[i] = std::clamp(out[i], -1.0f, 1.0f);

	_voices.erase(
		std::remove_if(
			_voices.begin(),
			_voices.end(),
			[](const Voice& voice) {
				return voice.stop_at_end &&
					   (voice.envelope_length == 0 || voice.envelope_position >= voice.envelope_length);
			}),
		_voices.end());
}

void AudioEngine::tick(long frame) {
	if (!_offline || frame <= _last_frame)
		return;
	const uint64_t target =
		static_cast<uint64_t>(std::llround(static_cast<double>(frame) * static_cast<double>(_sample_rate) / 60.0));
	uint64_t remaining = target > _rendered_frames ? target - _rendered_frames : 0;
	_last_frame = frame;

	std::vector<float> buffer;
	while (remaining > 0) {
		const int chunk = static_cast<int>(std::min<uint64_t>(remaining, 4096));
		buffer.resize(static_cast<size_t>(chunk) * 2);
		mix(buffer.data(), chunk);
		write_wav(buffer.data(), static_cast<size_t>(chunk));
		_rendered_frames += static_cast<uint64_t>(chunk);
		remaining -= static_cast<uint64_t>(chunk);
	}
}

void AudioEngine::write_wav(const float* samples, size_t frames) {
	if (!_wav_file)
		return;
	for (size_t i = 0; i < frames * 2; ++i) {
		const int16_t value = static_cast<int16_t>(std::clamp(samples[i], -1.0f, 1.0f) * 32767.0f);
		const uint8_t bytes[2] = {
			static_cast<uint8_t>(value & 0xFF),
			static_cast<uint8_t>((value >> 8) & 0xFF),
		};
		_wav_file.write(reinterpret_cast<const char*>(bytes), 2);
	}
	_wav_data_bytes += frames * 4;
}

void AudioEngine::stop_all() {
	std::lock_guard lock(_mutex);
	_voices.clear();
}

std::string AudioEngine::state_json() const {
	std::lock_guard lock(_mutex);
	size_t voices = 0;
	uint32_t bgm = 0;
	for (const auto& voice : _voices) {
		if (voice.bus != static_cast<uint8_t>(Bus::Bgm) || voice.stop_at_end)
			continue;
		++voices;
		bgm = voice.cue;
	}
	std::ostringstream out;
	out << "{\"active\":" << (active() ? "true" : "false") << ",\"realtime\":" << (realtime() ? "true" : "false")
		<< ",\"offline\":" << (offline() ? "true" : "false") << ",\"voices\":" << voices << ",\"bgm\":" << bgm
		<< ",\"events\":" << _events << ",\"rendered_frames\":" << _rendered_frames
		<< ",\"cache_bytes\":" << (_banks ? _banks->cache_bytes() : 0) << ",\"wav\":" << quoted(_options.wav_path)
		<< "}";
	return out.str();
}

std::string AudioEngine::summary() const {
	std::lock_guard lock(_mutex);
	size_t voices = 0;
	uint32_t bgm = 0;
	for (const auto& voice : _voices) {
		++voices;
		if (voice.bus == static_cast<uint8_t>(Bus::Bgm) && !voice.stop_at_end)
			bgm = voice.cue;
	}
	std::ostringstream out;
	out << "audio: ";
	if (!active())
		out << "off";
	else if (realtime())
		out << "realtime";
	else
		out << "offline";
	out << " voices=" << voices << " bgm=" << bgm << " events=" << _events << " rendered=" << _rendered_frames;
	return out.str();
}

} // namespace viewer
