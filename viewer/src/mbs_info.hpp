#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class AssetLoader;

// Parsed MBS track metadata shared by the runtime wait providers and the
// state report. The cache stores only names, run counts and durations, not the
// full SpriteData.
class MbsInfoCache {
public:
	struct Track {
		std::string name;
		long duration_frames = 0;
		uint32_t runs = 0;
		bool usable = false;
	};

	struct Info {
		bool found = false;
		bool parsed = false;
		std::vector<Track> tracks;

		int resolve(std::string_view name) const;
		long duration(int index) const;
		const Track* track(int index) const;
		int first_usable() const;
		int choose(int track_index, std::string_view name) const;
	};

	explicit MbsInfoCache(const AssetLoader& loader);

	const Info& get(const std::string& mbs) const;
	bool ftx_exists(const std::string& ftx, const std::string& mbs) const;

private:
	const AssetLoader& _loader;
	mutable std::unordered_map<std::string, Info> _cache;
	mutable std::unordered_map<std::string, bool> _ftx;
};
