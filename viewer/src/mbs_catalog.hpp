#pragma once

#include <impl/mbs/sections.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class AssetLoader;

// One parsed MBS shared by the logical runtime and the renderer. The catalog
// keeps track metadata for timing/continuation decisions and hands the parsed
// sections to the renderer on first upload, so the binary is parsed once.
class MbsCatalog {
public:
	struct Track {
		std::string name;
		long duration_frames = 0;
		long action_frames = 0;
		uint32_t runs = 0;
		bool usable = false;
		bool one_shot = false;
		int continuation_track = -1;
		std::string continuation_name;
		uint8_t transition_mode = 0;
	};

	struct Entry {
		bool found = false;
		bool parsed = false;
		std::vector<Track> tracks;
		std::optional<mbs::v77> sections;

		int resolve(std::string_view name) const;
		long duration(int index) const;
		const Track* track(int index) const;
		int first_usable() const;
		int choose(int track_index, std::string_view name) const;
	};

	explicit MbsCatalog(const AssetLoader& loader);

	const Entry& get(const std::string& mbs) const;

	// Move the parsed sections into the renderer. Metadata remains available.
	// A second request reparses the binary rather than retaining a duplicate.
	std::optional<mbs::v77> take_sections(const std::string& mbs) const;

	bool ftx_exists(const std::string& ftx, const std::string& mbs) const;

private:
	Entry& ensure(const std::string& mbs) const;
	std::optional<mbs::v77> read_sections(const std::string& mbs, bool& found) const;

	const AssetLoader& _loader;
	mutable std::unordered_map<std::string, Entry> _cache;
	mutable std::unordered_map<std::string, bool> _ftx;
};
