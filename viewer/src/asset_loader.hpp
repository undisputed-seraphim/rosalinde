#pragma once

#include "sprite_asset.hpp"
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

	// Decoded-but-not-uploaded asset data. `prepare` does the CPU-heavy work
	// (CPK extract + FTX decode) and is thread-safe; `make_asset` does the GL
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

	// Tolerant asset resolution: exact match first, then case-insensitive (the
	// game's VFS ignores case). resolve_ftx additionally zero-pads a single-digit
	// frame suffix and falls back to the MBS stem.
	std::optional<Job::dir_and_path> resolve(std::string_view path) const;
	std::optional<Job::dir_and_path> resolve_ftx(std::string_view ftx, std::string_view mbs) const;

	const cpk::archive& cpk() const { return _cpkt; }

private:
	cpk::archive _cpkt;
	std::unordered_map<std::string, Job::dir_and_path> _asset_index;
};
