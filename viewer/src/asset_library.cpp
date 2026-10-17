#include "asset_library.hpp"

#include <chrono>
#include <cstdio>
#include <utility>

std::optional<Job> AssetLibrary::job_for(const std::string& mbs, const std::string& ftx) const {
	auto mbs_path = _loader.resolve(mbs);
	if (!mbs_path)
		return std::nullopt;
	Job job;
	job.mbs = *mbs_path;
	auto ftx_path = ftx.empty() ? _loader.resolve_ftx(mbs, mbs) : _loader.resolve_ftx(ftx, mbs);
	if (!ftx_path)
		return std::nullopt;
	job.ftx = *ftx_path;
	return job;
}

SpriteAssetPtr
AssetLibrary::upload(AssetLoader::PreparedLayer&& prepared, const std::string& mbs, const std::string& ftx) {
	auto asset = _loader.make_asset(std::move(prepared), mbs, ftx, mbs, {});
	if (asset)
		_assets[key_for(mbs, ftx)] = asset;
	return asset;
}

SpriteAssetPtr AssetLibrary::get(const std::string& mbs, const std::string& ftx) {
	const std::string key = key_for(mbs, ftx);
	if (auto it = _assets.find(key); it != _assets.end())
		return it->second;
	if (auto it = _pending.find(key); it != _pending.end()) {
		try {
			auto prepared = it->second.future.get();
			Pending pending = std::move(it->second);
			_pending.erase(it);
			return upload(std::move(prepared), pending.mbs, pending.ftx);
		} catch (const std::exception& e) {
			std::fprintf(stderr, "[asset] %s: %s\n", key.c_str(), e.what());
			_pending.erase(it);
			_failed[key] = true;
			return {};
		}
	}
	if (_failed.contains(key))
		return {};
	auto job = job_for(mbs, ftx);
	if (!job) {
		_failed[key] = true;
		return {};
	}
	try {
		return upload(_loader.prepare(*job), mbs, ftx);
	} catch (const std::exception& e) {
		std::fprintf(stderr, "[asset] %s: %s\n", key.c_str(), e.what());
		_failed[key] = true;
		return {};
	}
}

SpriteAssetPtr AssetLibrary::try_get(const std::string& mbs, const std::string& ftx) {
	const std::string key = key_for(mbs, ftx);
	if (auto it = _assets.find(key); it != _assets.end())
		return it->second;
	auto it = _pending.find(key);
	if (it == _pending.end())
		return {};
	if (it->second.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return {};
	try {
		auto prepared = it->second.future.get();
		Pending pending = std::move(it->second);
		_pending.erase(it);
		return upload(std::move(prepared), pending.mbs, pending.ftx);
	} catch (const std::exception& e) {
		std::fprintf(stderr, "[asset] %s: %s\n", key.c_str(), e.what());
		_pending.erase(it);
		_failed[key] = true;
		return {};
	}
}

bool AssetLibrary::request(const std::string& mbs, const std::string& ftx) {
	const std::string key = key_for(mbs, ftx);
	if (_assets.contains(key) || _pending.contains(key) || _failed.contains(key))
		return _assets.contains(key);
	auto job = job_for(mbs, ftx);
	if (!job) {
		_failed[key] = true;
		return false;
	}
	_pending.emplace(
		key, Pending{std::async(std::launch::async, [this, job = *job] { return _loader.prepare(job); }), mbs, ftx});
	return true;
}

void AssetLibrary::request_paths(std::span<const std::string> paths) {
	for (const auto& path : paths) {
		if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".mbs") == 0)
			request(path, {});
	}
}
