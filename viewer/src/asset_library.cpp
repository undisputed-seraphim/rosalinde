#include "asset_library.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
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

SpriteAssetPtr AssetLibrary::upload(
	AssetLoader::PreparedLayer&& prepared,
	mbs::v77 sections,
	const std::string& mbs,
	const std::string& ftx) {
	auto asset = _loader.make_asset(std::move(prepared), std::move(sections), mbs, ftx, mbs, {});
	if (asset)
		_assets[key_for(mbs, ftx)] = asset;
	return asset;
}

SpriteAssetPtr AssetLibrary::get(const std::string& mbs, const std::string& ftx) {
	const std::string key = key_for(mbs, ftx);
	if (auto it = _assets.find(key); it != _assets.end()) {
		if (_trace)
			_trace(
				{_frame,
				 "asset",
				 "hit",
				 "\"key\":\"" + asb::trace::quote(key) + "\",\"mbs\":\"" + asb::trace::quote(mbs) + "\",\"ftx\":\"" +
					 asb::trace::quote(ftx) + "\""});
		return it->second;
	}
	if (_failed.contains(key))
		return {};

	const auto t0 = std::chrono::steady_clock::now();
	auto elapsed_ms = [&] {
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	};
	auto job = job_for(mbs, ftx);
	if (!job) {
		_failed[key] = true;
		return {};
	}
	try {
		auto sections = _mbs.take_sections(mbs);
		if (!sections) {
			spdlog::error("asset: {}: MBS sections unavailable", key);
			_failed[key] = true;
			return {};
		}
		auto asset = upload(_loader.prepare(*job), std::move(*sections), mbs, ftx);
		spdlog::debug("asset: load {} ftx={} {:.1f}ms", key, ftx, elapsed_ms());
		if (_trace && asset)
			_trace(
				{_frame,
				 "asset",
				 "load",
				 "\"key\":\"" + asb::trace::quote(key) + "\",\"mode\":\"sync\",\"mbs\":\"" + asb::trace::quote(mbs) +
					 "\",\"ftx\":\"" + asb::trace::quote(ftx) + "\",\"ms\":" + asb::trace::number(elapsed_ms())});
		return asset;
	} catch (const std::exception& e) {
		spdlog::error("asset: {}: {}", key, e.what());
		_failed[key] = true;
		return {};
	}
}
