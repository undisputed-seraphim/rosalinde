#include "mpd_asset.hpp"

#include "asset_loader.hpp"

#include <impl/mpd.hpp>

#include <spdlog/spdlog.h>

#include <optional>
#include <span>

namespace {

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
	const auto parsed = mpd::parse(data);
	if (!parsed)
		return {};

	auto textures = FTX::parse(std::span<char>(ftx_bytes->data(), ftx_bytes->size()));
	if (textures.empty())
		return {};

	auto asset = std::make_unique<MpdAsset>();
	asset->_renderer.upload_textures(textures, mpd);
	for (const auto& record : parsed->records) {
		const uint32_t texture = record.texture_index();
		if (texture >= textures.size())
			continue;
		const uint32_t first = static_cast<uint32_t>(asset->_verts.size());
		for (const auto& vertex : record.vertices) {
			const auto& tex_entry = textures[texture];
			SpriteVertex vert{};
			vert.texid = static_cast<int16_t>(texture);
			vert.xyz = {vertex.x, vertex.y, vertex.z};
			vert.color = vertex.color;
			vert.uv = {
				vertex.u * static_cast<float>(tex_entry.width),
				vertex.v * static_cast<float>(tex_entry.height)};
			asset->_verts.push_back(vert);
		}
		asset->_indices.insert(
			asset->_indices.end(), {first, first + 1, first + 3, first + 1, first + 2, first + 3});
	}
	if (asset->_verts.empty())
		return {};
	spdlog::debug(
		"mpd: {} quads, {} textures, {} records",
		asset->_indices.size() / 6,
		textures.size(),
		parsed->records.size());
	return asset;
}

void MpdAsset::draw(const glm::mat4& projection, const Camera& camera) {
	_renderer.draw_raw(_verts, _indices, projection * camera.lookAt());
}
