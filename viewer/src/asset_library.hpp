#pragma once

#include "asset_loader.hpp"
#include "sprite_asset.hpp"

#include <scarlett/trace.hpp>

#include <future>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

class AssetLibrary {
public:
	explicit AssetLibrary(const AssetLoader& loader)
		: _loader(loader) {}

	SpriteAssetPtr get(const std::string& mbs, const std::string& ftx);
	SpriteAssetPtr try_get(const std::string& mbs, const std::string& ftx);
	bool request(const std::string& mbs, const std::string& ftx);
	void request_paths(std::span<const std::string> paths);

	const AssetLoader& loader() const { return _loader; }

	void set_trace(asb::TraceCallback fn) { _trace = std::move(fn); }
	void set_frame(long frame) { _frame = frame; }

private:
	static std::string key_for(const std::string& mbs, const std::string& ftx) { return mbs + "|" + ftx; }
	std::optional<Job> job_for(const std::string& mbs, const std::string& ftx) const;
	SpriteAssetPtr upload(AssetLoader::PreparedLayer&& prepared, const std::string& mbs, const std::string& ftx);

	struct Pending {
		std::future<AssetLoader::PreparedLayer> future;
		std::string mbs;
		std::string ftx;
	};

	const AssetLoader& _loader;
	asb::TraceCallback _trace;
	long _frame = 0;
	std::unordered_map<std::string, SpriteAssetPtr> _assets;
	std::unordered_map<std::string, Pending> _pending;
	std::unordered_map<std::string, bool> _failed;
};
