#include <catch2/catch_all.hpp>

#include <criware/acb.hpp>
#include <criware/cpk.hpp>
#include <criware/hca.hpp>
#include <criware/nintendo_opus.hpp>
#include <criware/utf.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::filesystem::path cpk_path() {
	const char* path = std::getenv("UNICORN_CPK");
	return path ? std::filesystem::path(path) : std::filesystem::path();
}

std::vector<uint8_t> load_entry(const cpk::archive& cpk, const std::string& path, bool decipher) {
	const auto entry = cpk.find(path);
	if (entry == cpk.end())
		return {};
	std::vector<char> buffer;
	cpk.extract(*entry, buffer);
	if (decipher)
		utf::decipher(buffer);
	return {buffer.begin(), buffer.end()};
}

criware::ACB load_acb(const cpk::archive& cpk, const std::string& path) {
	auto data = load_entry(cpk, path, true);
	return criware::ACB::parse(std::span(data.data(), data.size()));
}

void bind_stream_awbs(const cpk::archive& cpk, criware::ACB& acb) {
	for (size_t port = 0; port < acb.stream_awb_names().size(); ++port) {
		const auto& name = acb.stream_awb_name(static_cast<uint16_t>(port));
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
		std::vector<uint8_t> header;
		cpk.extract_range(*entry, 0, 0x10 + static_cast<uint64_t>(count) * stride * 2, header);
		acb.bind_external_awb(static_cast<uint16_t>(port), header, entry->extract_size);
	}
}

std::vector<uint8_t>
waveform_bytes(const cpk::archive& cpk, const criware::ACB& acb, const criware::ACBWaveform& waveform) {
	const auto location = acb.locate_waveform(waveform);
	if (!location)
		return {};
	if (location->internal) {
		const auto raw = acb.raw_data();
		if (location->offset + location->size > raw.size())
			return {};
		return {
			raw.begin() + static_cast<ptrdiff_t>(location->offset),
			raw.begin() + static_cast<ptrdiff_t>(location->offset + location->size)};
	}
	const auto entry = cpk.find("Sound", acb.stream_awb_name(location->port) + ".awb");
	if (entry == cpk.end())
		return {};
	std::vector<uint8_t> out;
	cpk.extract_range(*entry, location->offset, location->size, out);
	return out;
}

} // namespace

TEST_CASE("utf optional columns can be absent") {
	if (cpk_path().empty())
		SKIP("UNICORN_CPK is not set");
	cpk::archive cpk{cpk_path()};
	auto data = load_entry(cpk, "Sound/bgm.acb", true);

	using Root = decltype(schema::make(schema::optional_col<"WaveformTable", utf::blob_ref>()));
	using Waveform =
		decltype(schema::make(schema::col<"EncodeType", uint8_t>(), schema::optional_col<"Id", uint16_t>()));

	const auto root = utf::table<Root>::parse(std::span(data.data(), data.size()));
	auto table_ref = utf::get<"WaveformTable">(root[0]);
	REQUIRE(table_ref);
	const auto waveforms = utf::table<Waveform>::parse(root.blob(*table_ref));
	CHECK(waveforms.size() == 130);
	CHECK(utf::get<"EncodeType">(waveforms[0]) == 24);
	CHECK_FALSE(utf::get<"Id">(waveforms[0]).has_value());
}

TEST_CASE("acb resolves cue chains") {
	if (cpk_path().empty())
		SKIP("UNICORN_CPK is not set");
	cpk::archive cpk{cpk_path()};

	auto bgm = load_acb(cpk, "Sound/bgm.acb");
	const auto* bgm_cue = bgm.find_cue(1);
	REQUIRE(bgm_cue != nullptr);
	REQUIRE(bgm_cue->waveform_identified);
	REQUIRE_FALSE(bgm_cue->waveform_indices.empty());
	const auto* bgm_waveform = bgm.waveform_at(bgm_cue->waveform_indices.front());
	REQUIRE(bgm_waveform != nullptr);
	CHECK(bgm_waveform->encode_type == 24);
	CHECK(bgm_waveform->streaming != 0);

	auto se = load_acb(cpk, "Sound/se.acb");
	const auto* se_cue = se.find_cue(1010);
	REQUIRE(se_cue != nullptr);
	REQUIRE_FALSE(se_cue->waveform_indices.empty());
	const auto* se_waveform = se.waveform_at(se_cue->waveform_indices.front());
	REQUIRE(se_waveform != nullptr);
	CHECK(se_waveform->encode_type == 2);
	CHECK(se_waveform->streaming == 0);

	auto voice = load_acb(cpk, "Sound/voice_jp.acb");
	const auto* voice_cue = voice.find_cue(1000101);
	REQUIRE(voice_cue != nullptr);
	REQUIRE(voice_cue->waveform_indices.size() == 1);
	const auto* voice_waveform = voice.waveform_at(voice_cue->waveform_indices.front());
	REQUIRE(voice_waveform != nullptr);
	CHECK(voice_waveform->encode_type == 24);
	CHECK(voice_waveform->streaming != 0);
	CHECK(voice.stream_awb_name(0) == "voice_jp");
}

TEST_CASE("afs2 external header and sizes") {
	if (cpk_path().empty())
		SKIP("UNICORN_CPK is not set");
	cpk::archive cpk{cpk_path()};
	auto acb = load_acb(cpk, "Sound/bgm.acb");
	bind_stream_awbs(cpk, acb);

	const auto* cue = acb.find_cue(1);
	REQUIRE(cue != nullptr);
	REQUIRE_FALSE(cue->waveform_indices.empty());
	const auto* waveform = acb.waveform_at(cue->waveform_indices.front());
	REQUIRE(waveform != nullptr);
	const auto location = acb.locate_waveform(*waveform);
	REQUIRE(location);
	REQUIRE_FALSE(location->internal);
	CHECK(location->size > 0);
	CHECK(location->offset + location->size <= cpk.find("Sound/bgm.awb")->extract_size);

	auto data = waveform_bytes(cpk, acb, *waveform);
	REQUIRE(data.size() == location->size);
	const auto info = criware::NintendoOpusDecoder::parse(data);
	REQUIRE(info);
	CHECK(info->channels == waveform->num_channels);
	CHECK(data.size() == static_cast<size_t>(info->data_offset) + 8 + info->data_size);
	CHECK(info->sample_rate == waveform->sampling_rate);
}

TEST_CASE("hca decodes an internal waveform") {
	if (cpk_path().empty())
		SKIP("UNICORN_CPK is not set");
	cpk::archive cpk{cpk_path()};
	auto acb = load_acb(cpk, "Sound/se.acb");
	const auto* cue = acb.find_cue(1010);
	REQUIRE(cue != nullptr);
	REQUIRE_FALSE(cue->waveform_indices.empty());
	const auto* waveform = acb.waveform_at(cue->waveform_indices.front());
	REQUIRE(waveform != nullptr);

	auto data = waveform_bytes(cpk, acb, *waveform);
	REQUIRE_FALSE(data.empty());
	criware::HcaDecoder decoder(data);
	REQUIRE(decoder.valid());
	CHECK(decoder.info().channels == waveform->num_channels);
	CHECK(decoder.info().sample_rate == waveform->sampling_rate);
	CHECK(
		data.size() == static_cast<size_t>(decoder.info().header_size) +
						   static_cast<size_t>(decoder.info().block_count) * decoder.info().block_size);

	auto pcm = decoder.decode_all(data);
	REQUIRE(pcm.size() == static_cast<size_t>(waveform->num_samples) * waveform->num_channels);
	float peak = 0.0f;
	for (float sample : pcm)
		peak = std::max(peak, std::abs(sample));
	CHECK(peak > 0.0f);
}

TEST_CASE("nintendo opus decodes an internal waveform") {
	if (cpk_path().empty())
		SKIP("UNICORN_CPK is not set");
	cpk::archive cpk{cpk_path()};
	auto acb = load_acb(cpk, "Sound/se.acb");
	const auto* cue = acb.find_cue(1000);
	REQUIRE(cue != nullptr);
	REQUIRE_FALSE(cue->waveform_indices.empty());
	const auto* waveform = acb.waveform_at(cue->waveform_indices.front());
	REQUIRE(waveform != nullptr);
	REQUIRE(waveform->encode_type == 24);

	auto data = waveform_bytes(cpk, acb, *waveform);
	REQUIRE_FALSE(data.empty());
	const auto info = criware::NintendoOpusDecoder::parse(data);
	REQUIRE(info);
	CHECK(info->channels == waveform->num_channels);
	CHECK(data.size() == static_cast<size_t>(info->data_offset) + 8 + info->data_size);

	auto pcm = criware::NintendoOpusDecoder::decode(data, static_cast<int32_t>(waveform->num_samples));
	REQUIRE(pcm.size() == static_cast<size_t>(waveform->num_samples) * waveform->num_channels);
	float peak = 0.0f;
	for (float sample : pcm)
		peak = std::max(peak, std::abs(sample));
	CHECK(peak > 0.0f);
}
