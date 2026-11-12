#include <boost/program_options.hpp>
#include <criware/acb.hpp>
#include <criware/cpk.hpp>
#include <criware/hca.hpp>
#include <criware/nintendo_opus.hpp>

#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
namespace po = boost::program_options;

namespace {

std::vector<uint8_t> load_entry(const cpk::archive& cpk, const std::string& path, bool decipher) {
	const auto entry = cpk.find(path);
	if (entry == cpk.end())
		throw std::runtime_error("CPK entry not found: " + path);
	std::vector<char> buffer;
	cpk.extract(*entry, buffer);
	if (decipher)
		utf::decipher(buffer);
	return std::vector<uint8_t>(buffer.begin(), buffer.end());
}

void bind_external_awbs(const cpk::archive& cpk, criware::ACB& acb) {
	for (size_t port = 0; port < acb.stream_awb_names().size(); ++port) {
		const auto& name = acb.stream_awb_name(static_cast<uint16_t>(port));
		if (name.empty())
			continue;
		const auto entry = cpk.find("Sound", name + ".awb");
		if (entry == cpk.end())
			continue;

		std::vector<uint8_t> base;
		cpk.extract_range(*entry, 0, 16, base);
		if (base.size() < 16 || std::memcmp(base.data(), "AFS2", 4) != 0)
			continue;
		const uint32_t version = static_cast<uint32_t>(base[4]) | (static_cast<uint32_t>(base[5]) << 8) |
								 (static_cast<uint32_t>(base[6]) << 16) | (static_cast<uint32_t>(base[7]) << 24);
		const uint32_t count = static_cast<uint32_t>(base[8]) | (static_cast<uint32_t>(base[9]) << 8) |
							   (static_cast<uint32_t>(base[10]) << 16) | (static_cast<uint32_t>(base[11]) << 24);
		const uint32_t stride = (version >> 8) & 0xFF;
		if (stride < 2 || stride > 8)
			continue;
		const uint64_t header_size = 0x10 + static_cast<uint64_t>(count) * stride * 2;
		std::vector<uint8_t> header;
		cpk.extract_range(*entry, 0, header_size, header);
		acb.bind_external_awb(static_cast<uint16_t>(port), header, entry->extract_size);
	}
}

std::vector<float>
read_waveform(const cpk::archive& cpk, const criware::ACB& acb, const criware::ACBWaveform& waveform) {
	const auto location = acb.locate_waveform(waveform);
	if (!location)
		throw std::runtime_error("waveform " + std::to_string(waveform.waveform_id) + " is not available");

	std::vector<uint8_t> data;
	if (location->internal) {
		const auto raw = acb.raw_data();
		if (location->offset + location->size > raw.size())
			throw std::runtime_error("internal waveform is out of range");
		data.assign(
			raw.begin() + static_cast<ptrdiff_t>(location->offset),
			raw.begin() + static_cast<ptrdiff_t>(location->offset + location->size));
	} else {
		const auto entry = cpk.find("Sound", acb.stream_awb_name(location->port) + ".awb");
		if (entry == cpk.end())
			throw std::runtime_error("external AWB is not available");
		cpk.extract_range(*entry, location->offset, location->size, data);
	}

	if (waveform.encode_type == 2) {
		criware::HcaDecoder decoder(data);
		if (!decoder.valid())
			throw std::runtime_error("HCA decode failed");
		return decoder.decode_all(data);
	}
	if (waveform.encode_type == 24) {
		auto decoded = criware::NintendoOpusDecoder::decode(data, static_cast<int32_t>(waveform.num_samples));
		if (decoded.empty())
			throw std::runtime_error("Opus decode failed");
		return decoded;
	}
	throw std::runtime_error("unsupported encode type " + std::to_string(waveform.encode_type));
}

std::vector<float>
decode_indices(const cpk::archive& cpk, const criware::ACB& acb, const std::vector<uint16_t>& indices) {
	std::vector<std::vector<float>> parts;
	size_t frames = 0;
	uint32_t channels = 1;
	for (uint16_t index : indices) {
		const auto* waveform = acb.waveform_at(index);
		if (!waveform)
			continue;
		auto samples = read_waveform(cpk, acb, *waveform);
		const uint32_t source_channels = waveform->num_channels != 0 ? waveform->num_channels : 1;
		if (parts.empty()) {
			channels = source_channels;
		} else if (source_channels != channels) {
			continue;
		}
		frames = std::max(frames, samples.size() / channels);
		parts.push_back(std::move(samples));
	}

	std::vector<float> mixed(frames * 2, 0.0f);
	for (const auto& part : parts) {
		const size_t count = part.size() / channels;
		for (size_t i = 0; i < count; ++i) {
			const float left = part[i * channels];
			const float right = channels > 1 ? part[i * channels + 1] : left;
			mixed[i * 2] += left;
			mixed[i * 2 + 1] += right;
		}
	}
	for (float& sample : mixed)
		sample = std::clamp(sample, -1.0f, 1.0f);
	return mixed;
}

void write_wav(const std::string& path, const std::vector<float>& samples, uint32_t sample_rate = 48000) {
	std::ofstream out(path, std::ios::binary);
	if (!out)
		throw std::runtime_error("cannot write " + path);
	const uint32_t data_size = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
	const uint16_t channels = 2;
	const uint16_t bits = 16;
	const uint32_t byte_rate = sample_rate * channels * bits / 8;
	const uint16_t block_align = channels * bits / 8;
	auto u16 = [&](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
	auto u32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
	out.write("RIFF", 4);
	u32(36 + data_size);
	out.write("WAVEfmt ", 8);
	u32(16);
	u16(1);
	u16(channels);
	u32(sample_rate);
	u32(byte_rate);
	u16(block_align);
	u16(bits);
	out.write("data", 4);
	u32(data_size);
	for (float sample : samples) {
		const int16_t value = static_cast<int16_t>(std::clamp(sample, -1.0f, 1.0f) * 32767.0f);
		out.write(reinterpret_cast<const char*>(&value), 2);
	}
}

void play(const std::vector<float>& samples) {
	if (!SDL_Init(SDL_INIT_AUDIO)) {
		std::cerr << "SDL_Init(AUDIO) failed: " << SDL_GetError() << "\n";
		return;
	}
	SDL_AudioSpec spec{};
	spec.format = SDL_AUDIO_F32;
	spec.channels = 2;
	spec.freq = 48000;
	auto* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
	if (!stream) {
		std::cerr << "SDL_OpenAudioDeviceStream failed: " << SDL_GetError() << "\n";
		SDL_Quit();
		return;
	}
	SDL_ResumeAudioStreamDevice(stream);
	constexpr size_t chunk_samples = 4096;
	size_t offset = 0;
	while (offset < samples.size()) {
		const size_t count = std::min(chunk_samples, samples.size() - offset);
		SDL_PutAudioStreamData(stream, samples.data() + offset, static_cast<int>(count * sizeof(float)));
		offset += count;
		while (SDL_GetAudioStreamQueued(stream) > static_cast<int>(chunk_samples * 4))
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	while (SDL_GetAudioStreamQueued(stream) > 0)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	SDL_DestroyAudioStream(stream);
	SDL_Quit();
}

} // namespace

int main(int argc, char* argv[]) try {
	std::string cpk_path;
	std::string acb_path;
	int cue_id = -1;
	int track_idx = -1;
	bool list = false;
	std::string wav_path;

	po::options_description desc;
	desc.add_options()("help,h", "Print this help message")(
		"cpk", po::value<std::string>(&cpk_path)->required(), "Path to Unicorn.CPK")(
		"acb", po::value<std::string>(&acb_path)->required(), "ACB path inside CPK (e.g. Sound/bgm.acb)")(
		"cue,c", po::value<int>(&cue_id), "Play cue by ID")(
		"track,t", po::value<int>(&track_idx), "Play track by index")(
		"list,l", po::bool_switch(&list), "List all cues")(
		"wav,w", po::value<std::string>(&wav_path), "Write decoded audio to a WAV file");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << '\n';
		return 1;
	}

	cpk::archive cpk{fs::path(cpk_path)};
	auto acb_data = load_entry(cpk, acb_path, true);
	auto acb = criware::ACB::parse(std::span(acb_data.data(), acb_data.size()));
	bind_external_awbs(cpk, acb);

	std::cout << acb_path << ": " << acb.cues().size() << " cues, " << acb.waveforms().size() << " waveforms, "
			  << acb.tracks().size() << " tracks\n";
	std::cout << "format version: 0x" << std::hex << acb.format_version() << std::dec << "\n";

	if (list) {
		for (const auto& cue : acb.cues()) {
			std::cout << "  cue " << cue.cue_id << " refType=" << static_cast<int>(cue.reference_type)
					  << " wav=" << cue.waveform_id << " enc=" << static_cast<int>(cue.encode_type)
					  << " stream=" << cue.streaming << " len=" << cue.length;
			if (!cue.cue_name.empty())
				std::cout << " name=" << cue.cue_name;
			std::cout << "\n";
		}
		for (const auto& waveform : acb.waveforms())
			std::cout << "  wav " << waveform.waveform_id << " enc=" << static_cast<int>(waveform.encode_type)
					  << " ch=" << static_cast<int>(waveform.num_channels) << " rate=" << waveform.sampling_rate
					  << " samples=" << waveform.num_samples << " stream=" << static_cast<int>(waveform.streaming)
					  << " loop=" << static_cast<int>(waveform.loop_flag) << " loopRange=" << waveform.loop_start
					  << ".." << waveform.loop_end << "\n";
		return 0;
	}

	std::vector<uint16_t> indices;
	if (cue_id >= 0) {
		const auto* cue = acb.find_cue(static_cast<uint32_t>(cue_id));
		if (!cue || cue->waveform_indices.empty()) {
			std::cerr << "cue " << cue_id << " not found or has no waveform\n";
			return 1;
		}
		indices = cue->waveform_indices;
		std::cout << "Decoding cue " << cue_id << " (" << indices.size() << " waveform(s))\n";
	} else if (track_idx >= 0) {
		const auto* track = acb.find_track(static_cast<uint32_t>(track_idx));
		if (!track || track->waveform_indices.empty()) {
			std::cerr << "track " << track_idx << " not found or has no waveform\n";
			return 1;
		}
		indices = track->waveform_indices;
		std::cout << "Decoding track " << track_idx << " (" << indices.size() << " waveform(s))\n";
	} else {
		std::cerr << "No cue or track specified. Use --cue <id> or --track <idx>\n";
		return 1;
	}

	auto samples = decode_indices(cpk, acb, indices);
	std::cout << "Decoded " << samples.size() / 2 << " stereo frames ("
			  << (static_cast<double>(samples.size()) / 2.0 / 48000.0) << "s)\n";
	if (!wav_path.empty())
		write_wav(wav_path, samples);
	if (wav_path.empty())
		play(samples);
	return 0;
} catch (const std::exception& e) {
	std::cerr << "Error: " << e.what() << '\n';
	return 1;
}
