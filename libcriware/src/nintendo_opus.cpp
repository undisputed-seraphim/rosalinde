#include <criware/nintendo_opus.hpp>

#include <opus.h>

#include <algorithm>
#include <cstring>

namespace criware {
namespace {

constexpr uint32_t opus_basic_header = 0x80000001;
constexpr uint32_t opus_data_header = 0x80000004;
constexpr uint32_t opus_context_header = 0x80000003;

uint32_t read_le32(const uint8_t* p) {
	return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
		   (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t read_le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

uint32_t read_be32(const uint8_t* p) {
	return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
		   (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

} // namespace

std::optional<NintendoOpusInfo> NintendoOpusDecoder::parse(std::span<const uint8_t> data) {
	if (data.size() < 0x20 || read_le32(data.data()) != opus_basic_header)
		return std::nullopt;

	NintendoOpusInfo info;
	info.channels = data[9];
	info.sample_rate = read_le32(data.data() + 0x0c);
	if (info.sample_rate == 0)
		info.sample_rate = 48000;
	info.skip = read_le16(data.data() + 0x1c);

	const uint32_t data_offset = read_le32(data.data() + 0x10);
	if (data_offset > data.size() || data.size() - data_offset < 8)
		return std::nullopt;
	if (read_le32(data.data() + data_offset) != opus_data_header)
		return std::nullopt;
	info.data_offset = data_offset;
	info.data_size = read_le32(data.data() + data_offset + 4);
	if (info.data_size > data.size() - data_offset - 8)
		return std::nullopt;

	const uint32_t context_offset = read_le32(data.data() + 0x18);
	if (context_offset != 0 && context_offset + 0x18 <= data.size() &&
		read_le32(data.data() + context_offset) == opus_context_header) {
		info.loop_enabled = data[context_offset + 0x09] != 0;
		info.num_samples = read_le32(data.data() + context_offset + 0x0c);
		info.loop_start = read_le32(data.data() + context_offset + 0x10);
		info.loop_end = read_le32(data.data() + context_offset + 0x14);
	}

	if (info.channels == 0 || info.channels > 2)
		return std::nullopt;
	return info;
}

std::vector<float> NintendoOpusDecoder::decode(std::span<const uint8_t> data, int32_t expected_samples) {
	std::vector<float> out;
	const auto info = parse(data);
	if (!info)
		return out;

	int error = 0;
	OpusDecoder* decoder = opus_decoder_create(48000, info->channels, &error);
	if (!decoder || error != OPUS_OK)
		return out;

	const size_t start = info->data_offset + 8;
	const size_t end = start + info->data_size;
	size_t pos = start;
	uint32_t skip = info->skip;
	std::vector<float> packet(5760 * info->channels);

	while (pos + 8 <= end) {
		const uint32_t packet_size = read_be32(data.data() + pos);
		if (packet_size == 0 || packet_size > 0xFFFFF || packet_size > end - pos - 8)
			break;
		const uint8_t* payload = data.data() + pos + 8;
		const int frames =
			opus_decode_float(decoder, payload, static_cast<opus_int32>(packet_size), packet.data(), 5760, 0);
		if (frames < 0)
			break;

		size_t first = 0;
		if (skip > 0) {
			const size_t drop = std::min<size_t>(skip, static_cast<size_t>(frames));
			first = drop;
			skip -= static_cast<uint32_t>(drop);
		}
		for (size_t i = first; i < static_cast<size_t>(frames); ++i)
			for (uint32_t channel = 0; channel < info->channels; ++channel)
				out.push_back(packet[i * info->channels + channel]);
		pos += 8 + packet_size;
	}

	opus_decoder_destroy(decoder);

	if (expected_samples > 0) {
		const size_t samples = static_cast<size_t>(expected_samples);
		const size_t available = out.size() / info->channels;
		if (available > samples)
			out.resize(samples * info->channels);
	}
	return out;
}

} // namespace criware
