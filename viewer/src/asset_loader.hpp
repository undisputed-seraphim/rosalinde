#pragma once

#include "sprite_asset.hpp"
#include "sprite_layer.hpp"
#include "tables.hpp"

#include <criware/cpk.hpp>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

class AssetLoader {
public:
	AssetLoader(std::filesystem::path cpk_path);

	std::unique_ptr<SpriteLayer>
	load_layer(const Job& job, uint32_t trackid, const std::string& class_name, const std::string& variant_name) const;

	// Decoded-but-not-uploaded asset data. `prepare` does the CPU-heavy work
	// (CPK extract + FTX decode) and is thread-safe; `make_layer` does the GL
	// upload and must run on the GL thread.
	struct PreparedLayer {
		std::vector<char> mbs;
		std::vector<FTX::Entry> ftx;
	};
	PreparedLayer prepare(const Job& job) const;
	SpriteAssetPtr make_asset(
		PreparedLayer&& prepared,
		const std::string& mbs,
		const std::string& ftx,
		const std::string& class_name,
		const std::string& variant_name) const;
	std::unique_ptr<SpriteLayer> make_layer(
		PreparedLayer&& prepared,
		uint32_t trackid,
		const std::string& class_name,
		const std::string& variant_name) const;

	BackgroundScene load_background(const Job& job) const;

	// Tolerant asset resolution: exact match first, then case-insensitive (the
	// game's VFS ignores case). resolve_ftx additionally zero-pads a single-digit
	// frame suffix and falls back to the MBS stem.
	std::optional<Job::dir_and_path> resolve(std::string_view path) const;
	std::optional<Job::dir_and_path> resolve_ftx(std::string_view ftx, std::string_view mbs) const;

	const std::vector<std::string>& class_names() const { return _class_names; }

	const cpk::archive& cpk() const { return _cpkt; }

private:
	cpk::archive _cpkt;
	std::vector<std::string> _class_names;
	std::unordered_map<std::string, Job::dir_and_path> _asset_index;
};
