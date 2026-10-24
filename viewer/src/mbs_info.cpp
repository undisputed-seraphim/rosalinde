#include "mbs_info.hpp"

#include "asset_loader.hpp"
#include "sprite.hpp"

#include <criware/cpk.hpp>

#include <span>
#include <utility>

MbsInfoCache::MbsInfoCache(const AssetLoader& loader)
	: _loader(loader) {}

int MbsInfoCache::Info::resolve(std::string_view name) const {
	for (size_t i = 0; i < tracks.size(); ++i)
		if (tracks[i].name == name)
			return static_cast<int>(i);
	return -1;
}

long MbsInfoCache::Info::duration(int index) const {
	const Track* found = track(index);
	return found ? found->duration_frames : 0;
}

const MbsInfoCache::Track* MbsInfoCache::Info::track(int index) const {
	if (index < 0 || static_cast<size_t>(index) >= tracks.size())
		return nullptr;
	return &tracks[static_cast<size_t>(index)];
}

int MbsInfoCache::Info::first_usable() const {
	for (size_t i = 0; i < tracks.size(); ++i)
		if (tracks[i].usable)
			return static_cast<int>(i);
	return -1;
}

int MbsInfoCache::Info::choose(int track_index, std::string_view name) const {
	if (const Track* selected = track(track_index); selected && selected->usable)
		return track_index;
	if (!name.empty()) {
		const int by_name = resolve(name);
		if (const Track* selected = track(by_name); selected && selected->usable)
			return by_name;
	}
	return first_usable();
}

const MbsInfoCache::Info& MbsInfoCache::get(const std::string& mbs) const {
	auto found = _cache.find(mbs);
	if (found != _cache.end())
		return found->second;

	Info info;
	if (!mbs.empty()) {
		try {
			if (auto path = _loader.resolve(mbs)) {
				const cpk::archive& archive = _loader.cpk();
				if (auto entry = archive.find(path->dir, path->path); entry != archive.end()) {
					info.found = true;
					std::vector<char> bytes;
					archive.extract(*entry, bytes);
					SpriteData data = SpriteData::load(
						std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()),
						{});
					info.parsed = true;
					info.tracks.reserve(data.tracks.size());
					for (size_t i = 0; i < data.tracks.size(); ++i) {
						const SpriteData::Track& track = data.tracks[i];
						std::string continuation_name;
						if (track.next_track >= 0 && static_cast<size_t>(track.next_track) < data.tracks.size())
							continuation_name = data.tracks[static_cast<size_t>(track.next_track)].name;
						info.tracks.push_back(Track{
							track.name,
							track_duration_frames(data, static_cast<int>(i)),
							static_cast<uint32_t>(track.runs.size()),
							!track.disabled && !track.runs.empty(),
							track.one_shot,
							track.next_track,
							std::move(continuation_name)});
					}
				}
			}
		} catch (...) {
			info.parsed = false;
		}
	}
	return _cache.emplace(mbs, std::move(info)).first->second;
}

bool MbsInfoCache::ftx_exists(const std::string& ftx, const std::string& mbs) const {
	if (ftx.empty())
		return false;
	const std::string key = mbs + '\x1f' + ftx;
	auto found = _ftx.find(key);
	if (found != _ftx.end())
		return found->second;
	const bool exists = _loader.resolve_ftx(ftx, mbs).has_value();
	_ftx.emplace(key, exists);
	return exists;
}
