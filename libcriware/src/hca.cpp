#include <criware/hca.hpp>

extern "C" {
#include "hca/clhca.h"
}

#include <algorithm>
#include <cstring>

namespace criware {

struct HcaDecoder::Impl {
	clHCA* state = nullptr;
	HcaInfo info;
	std::vector<uint8_t> scratch;
	std::vector<float> block;

	~Impl() {
		if (state)
			clHCA_delete(state);
	}
};

HcaDecoder::HcaDecoder(std::span<const uint8_t> data, uint64_t key) {
	if (data.size() < 8)
		return;

	const int header_size = clHCA_isOurFile(data.data(), static_cast<unsigned int>(data.size()));
	if (header_size <= 0)
		return;

	auto impl = std::make_unique<Impl>();
	impl->state = clHCA_new();
	if (!impl->state)
		return;

	clHCA_SetKey(impl->state, key);
	if (clHCA_DecodeHeader(
			impl->state, data.data(), static_cast<unsigned int>(std::min<size_t>(data.size(), header_size))) != 0) {
		clHCA_delete(impl->state);
		return;
	}

	clHCA_stInfo info{};
	if (clHCA_getInfo(impl->state, &info) != 0) {
		clHCA_delete(impl->state);
		return;
	}

	impl->info.version = info.version;
	impl->info.header_size = info.headerSize;
	impl->info.sample_rate = info.samplingRate;
	impl->info.channels = info.channelCount;
	impl->info.block_size = info.blockSize;
	impl->info.block_count = info.blockCount;
	impl->info.samples_per_block = info.samplesPerBlock;
	impl->info.encoder_delay = info.encoderDelay;
	impl->info.encoder_padding = info.encoderPadding;
	impl->info.sample_count = info.sampleCount;
	impl->info.loop_enabled = info.loopEnabled != 0;
	impl->info.loop_start_block = info.loopStartBlock;
	impl->info.loop_end_block = info.loopEndBlock;
	impl->info.loop_start_sample = info.loopStartSample;
	impl->info.loop_end_sample = info.loopEndSample;
	impl->info.cipher_type = info.encryptionEnabled ? 56 : 0;
	impl->info.rva_volume = 1.0f;

	if (impl->info.channels == 0 || impl->info.block_size == 0)
		return;

	impl->scratch.resize(impl->info.block_size);
	impl->block.resize(static_cast<size_t>(impl->info.samples_per_block) * impl->info.channels);
	_impl = std::move(impl);
}

HcaDecoder::~HcaDecoder() = default;
HcaDecoder::HcaDecoder(HcaDecoder&&) noexcept = default;
HcaDecoder& HcaDecoder::operator=(HcaDecoder&&) noexcept = default;

std::optional<HcaInfo> HcaDecoder::parse(std::span<const uint8_t> data, uint64_t key) {
	HcaDecoder decoder(data, key);
	if (!decoder.valid())
		return std::nullopt;
	return decoder.info();
}

bool HcaDecoder::valid() const { return _impl != nullptr; }

const HcaInfo& HcaDecoder::info() const {
	static const HcaInfo empty;
	return _impl ? _impl->info : empty;
}

size_t HcaDecoder::decode_block(std::span<const uint8_t> block, float* out) {
	if (!_impl || block.size() < _impl->info.block_size)
		return 0;
	std::memcpy(_impl->scratch.data(), block.data(), _impl->info.block_size);
	if (clHCA_DecodeBlock(_impl->state, _impl->scratch.data(), _impl->info.block_size) < 0)
		return 0;
	clHCA_ReadSamples(_impl->state, out);
	return _impl->info.samples_per_block;
}

std::vector<float> HcaDecoder::decode_all(std::span<const uint8_t> data) {
	std::vector<float> out;
	if (!_impl)
		return out;
	const auto& info = _impl->info;
	if (data.size() < static_cast<size_t>(info.header_size) + static_cast<size_t>(info.block_count) * info.block_size)
		return out;

	const size_t total_frames = static_cast<size_t>(info.block_count) * info.samples_per_block;
	std::vector<float> decoded(total_frames * info.channels);
	for (uint32_t i = 0; i < info.block_count; ++i) {
		const size_t offset = info.header_size + static_cast<size_t>(i) * info.block_size;
		if (decode_block(
				data.subspan(offset, info.block_size),
				decoded.data() + static_cast<size_t>(i) * info.samples_per_block * info.channels) == 0)
			return {};
	}

	const size_t start = std::min<size_t>(info.encoder_delay, total_frames);
	const size_t available = total_frames - start;
	const size_t count = info.sample_count != 0 ? std::min<size_t>(info.sample_count, available) : available;
	out.resize(count * info.channels);
	std::memcpy(out.data(), decoded.data() + start * info.channels, out.size() * sizeof(float));
	return out;
}

} // namespace criware
