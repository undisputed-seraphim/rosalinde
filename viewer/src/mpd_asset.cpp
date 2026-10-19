#include "mpd_asset.hpp"

#include "asset_loader.hpp"

#include <spdlog/spdlog.h>

#include <cstring>
#include <optional>
#include <span>

namespace {

uint32_t read_u32(std::span<const uint8_t> data, size_t offset) {
	uint32_t value = 0;
	if (offset + sizeof(value) <= data.size())
		std::memcpy(&value, data.data() + offset, sizeof(value));
	return value;
}

float read_f32(std::span<const uint8_t> data, size_t offset) {
	float value = 0;
	if (offset + sizeof(value) <= data.size())
		std::memcpy(&value, data.data() + offset, sizeof(value));
	return value;
}

std::optional<std::vector<char>> extract(const AssetLoader& loader, const std::string& path) {
	auto resolved = loader.resolve(path);
	if (!resolved)
		return std::nullopt;
	const auto& cpk = loader.cpk();
	auto entry = cpk.find(resolved->dir, resolved->path);
	if (entry == cpk.end())
		return std::nullopt;
	std::vector<char> bytes;
	cpk.extract(*entry, bytes);
	return bytes;
}

} // namespace

std::unique_ptr<MpdAsset> MpdAsset::load(const AssetLoader& loader, const std::string& mpd, const std::string& ftx) {
	auto mpd_bytes = extract(loader, mpd);
	auto ftx_bytes = extract(loader, ftx);
	if (!mpd_bytes || !ftx_bytes)
		return {};

	const std::span<const uint8_t> data(
		reinterpret_cast<const uint8_t*>(mpd_bytes->data()), mpd_bytes->size());
	if (data.size() < 0x20 || std::memcmp(data.data(), "MPD\0", 4) != 0)
		return {};

	auto textures = FTX::parse(std::span<char>(ftx_bytes->data(), ftx_bytes->size()));
	if (textures.empty())
		return {};

	auto asset = std::make_unique<MpdAsset>();
	asset->_renderer.upload_textures(textures, mpd);
	const uint32_t header_count = read_u32(data, 0x14);
	const uint32_t count = header_count ? header_count : static_cast<uint32_t>((data.size() - 0x20) / 0xA0);
	for (uint32_t i = 0; i < count; ++i) {
		const size_t base = 0x20 + static_cast<size_t>(i) * 0xA0;
		if (base + 0xA0 > data.size())
			break;
		const uint32_t texture = read_u32(data, base + 0x04) & 0xFF;
		if (texture >= textures.size())
			continue;
		const uint32_t first = static_cast<uint32_t>(asset->_verts.size());
		for (int v = 0; v < 4; ++v) {
			const size_t at = base + 0x08 + static_cast<size_t>(v) * 0x18;
			SpriteVertex vert{};
			vert.texid = static_cast<int16_t>(texture);
			vert.xyz = {read_f32(data, at), read_f32(data, at + 4), read_f32(data, at + 8)};
			vert.color = read_u32(data, at + 0x0c);
			const auto& tex_entry = textures[texture];
			vert.uv = {
				read_f32(data, at + 0x10) * static_cast<float>(tex_entry.width),
				read_f32(data, at + 0x14) * static_cast<float>(tex_entry.height)};
			asset->_verts.push_back(vert);
		}
		asset->_indices.insert(
			asset->_indices.end(), {first, first + 1, first + 3, first + 1, first + 2, first + 3});
	}
	if (asset->_verts.empty())
		return {};
	spdlog::debug("mpd: {} quads, {} textures", asset->_indices.size() / 6, textures.size());
	return asset;
}

void MpdAsset::draw(const glm::mat4& projection, const Camera& camera) {
	_renderer.draw_raw(_verts, _indices, projection * camera.lookAt());
}
