#include <boost/program_options.hpp>
#include <criware/acb.hpp>
#include <criware/audio_source.hpp>
#include <criware/cpk.hpp>

#include <SDL3/SDL.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
namespace po = boost::program_options;

static std::vector<char> load_acb(std::string_view cpk_path, std::string_view acb_path) {
	auto cpk = TopLevelCpk(fs::path(cpk_path)).getTableOfContents();
	auto pos = acb_path.find_last_of('/');
	std::string_view dir = (pos == std::string_view::npos) ? "" : acb_path.substr(0, pos);
	std::string_view name = (pos == std::string_view::npos) ? acb_path : acb_path.substr(pos + 1);

	auto it = cpk.find_file(dir, name);
	if (it == cpk.end()) {
		throw std::runtime_error("ACB file not found: " + std::string(acb_path));
	}

	std::vector<char> buf;
	cpk.extract(*it, buf);
	UTF::decipher(buf);
	return buf;
}

int main(int argc, char* argv[]) try {
	std::string cpk_path;
	std::string acb_path;
	int cue_id = -1;
	int track_idx = -1;
	bool list = false;

	po::options_description desc;
	desc.add_options()
		("help,h", "Print this help message")
		("cpk", po::value<std::string>(&cpk_path)->required(), "Path to Unicorn.CPK")
		("acb", po::value<std::string>(&acb_path)->required(), "ACB path inside CPK (e.g. Sound/bgm.acb)")
		("cue,c", po::value<int>(&cue_id), "Play cue by ID")
		("track,t", po::value<int>(&track_idx), "Play track by index")
		("list,l", po::bool_switch(&list), "List all cues");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << '\n';
		return 1;
	}

	auto acb_data = load_acb(cpk_path, acb_path);
	auto acb = criware::ACB::parse(std::span(
		reinterpret_cast<const uint8_t*>(acb_data.data()), acb_data.size()));

	std::cout << acb_path << ": " << acb.cues().size() << " cues, "
			  << acb.waveforms().size() << " waveforms, "
			  << acb.tracks().size() << " tracks\n";
	std::cout << "format version: 0x" << std::hex << acb.format_version() << std::dec << "\n";

	unsigned named = 0;
	for (auto& c : acb.cues())
		if (!c.cue_name.empty()) named++;
	std::cout << named << " named cues\n";

	if (list) {
		for (auto& c : acb.cues()) {
			std::cout << "  cue " << c.cue_id << " refType=" << (int)c.reference_type
					  << " wav=" << c.waveform_id << " enc=" << (int)c.encode_type
					  << " stream=" << c.streaming;
			if (!c.cue_name.empty()) std::cout << " name=" << c.cue_name;
			std::cout << "\n";
		}
		unsigned wav_count = 0;
		for (auto& w : acb.waveforms()) {
			if (wav_count++ < 5 || wav_count == acb.waveforms().size())
				std::cout << "  wav " << w.waveform_id << " enc=" << (int)w.encode_type
						  << " ch=" << (int)w.num_channels << " rate=" << w.sampling_rate
						  << " samples=" << w.num_samples << " stream=" << (int)w.streaming << "\n";
		}
		return 0;
	}

	// Select which waveform to play
	const criware::ACBWaveform* target = nullptr;
	uint16_t waveform_id = 0;

	if (cue_id >= 0) {
		auto* cue = acb.find_cue(static_cast<uint32_t>(cue_id));
		if (!cue || !cue->waveform_identified) {
			std::cerr << "cue " << cue_id << " not found or not resolved\n";
			return 1;
		}
		std::cout << "Playing cue " << cue_id << " (wav=" << cue->waveform_id
				  << " enc=" << (int)cue->encode_type << " stream=" << cue->streaming << ")\n";
		waveform_id = cue->waveform_id;
	} else if (track_idx >= 0) {
		auto* track = acb.find_track(static_cast<uint32_t>(track_idx));
		if (!track || !track->waveform_identified) {
			std::cerr << "track " << track_idx << " not found or not resolved\n";
			return 1;
		}
		std::cout << "Playing track " << track_idx << " (wav=" << track->waveform_id
				  << " enc=" << (int)track->encode_type << " stream=" << track->streaming << ")\n";
		waveform_id = track->waveform_id;
	} else {
		std::cout << "No cue or track specified. Use --cue <id> or --track <idx>\n";
		std::cout << desc << '\n';
		return 1;
	}

	target = acb.find_waveform(waveform_id);
	if (!target) {
		std::cerr << "waveform " << waveform_id << " not found\n";
		return 1;
	}
	if (target->streaming) {
		std::cerr << "streaming waveforms not yet supported\n";
		return 1;
	}

	auto span = acb.waveform_span(waveform_id);
	if (span.empty()) {
		std::cerr << "could not extract waveform data\n";
		return 1;
	}

	criware::SpanAudioSource source(span);

	// SDL3 audio playback
	if (!SDL_Init(SDL_INIT_AUDIO)) {
		std::cerr << "SDL_Init(AUDIO) failed: " << SDL_GetError() << "\n";
		return 1;
	}

	SDL_AudioSpec spec{};
	spec.format = SDL_AUDIO_S16LE;
	spec.channels = 2;
	spec.freq = 44100;

	auto* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
	if (!stream) {
		std::cerr << "SDL_OpenAudioDeviceStream failed: " << SDL_GetError() << "\n";
		return 1;
	}

	SDL_ResumeAudioStreamDevice(stream);

	std::cout << "Audio device: " << spec.freq << "Hz, " << spec.channels << "ch, "
			  << SDL_AUDIO_BYTESIZE(spec.format) * 8 << "-bit\n";
	std::cout << "Total waveform size: " << source.total_size() << " bytes\n";

	// Push audio data in chunks
	constexpr size_t CHUNK = 4096;
	uint8_t chunk[CHUNK];
	size_t total_pushed = 0;

	while (true) {
		size_t n = source.read(chunk, CHUNK);
		if (n == 0) break;

		SDL_PutAudioStreamData(stream, chunk, static_cast<int>(n));
		total_pushed += n;

		// Wait if too much data is queued
		while (SDL_GetAudioStreamQueued(stream) > static_cast<int>(CHUNK * 4)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}

	std::cout << "Pushed " << total_pushed << " bytes, waiting for playback...\n";

	// Wait for playback to finish
	while (SDL_GetAudioStreamQueued(stream) > 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	SDL_DestroyAudioStream(stream);
	std::cout << "Done.\n";

} catch (const std::exception& e) {
	std::cerr << "Error: " << e.what() << '\n';
	return 1;
}
