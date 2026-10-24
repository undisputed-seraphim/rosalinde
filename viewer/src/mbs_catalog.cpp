#include "mbs_catalog.hpp"

#include "asset_loader.hpp"
#include "sprite.hpp"

#include <criware/cpk.hpp>

#include <span>
#include <utility>

MbsCatalog::MbsCatalog(const AssetLoader& loader)
	: _loader(loader) {}

int MbsCatalog::Entry::resolve(std::string_view name) const {
	for (size_t i = 0; i < tracks.size(); ++i)
		if (tracks[i].name == name)
			return static_cast<int>(i);
	return -1;
}

long MbsCatalog::Entry::duration(int index) const {
	const Track* found = track(index);
	return found ? found->duration_frames : 0;
}

const MbsCatalog::Track* MbsCatalog::Entry::track(int index) const {
	if (index < 0 || static_cast<size_t>(index) >= tracks.size())
		return nullptr;
	return &tracks[static_cast<size_t>(index)];
}

int MbsCatalog::Entry::first_usable() const {
	for (size_t i = 0; i < tracks.size(); ++i)
		if (tracks[i].usable)
			return static_cast<int>(i);
	return -1;
}

int MbsCatalog::Entry::choose(int track_index, std::string_view name) const {
	if (const Track* selected = track(track_index); selected && selected->usable)
		return track_index;
	if (!name.empty()) {
		const int by_name = resolve(name);
		if (const Track* selected = track(by_name); selected && selected->usable)
			return by_name;
	}
	return first_usable();
}

std::optional<mbs::v77> MbsCatalog::read_sections(const std::string& mbs, bool& found) const {
	const auto path = _loader.resolve(mbs);
	if (!path)
		return std::nullopt;
	const cpk::archive& archive = _loader.cpk();
	const auto entry = archive.find(path->dir, path->path);
	if (entry == archive.end())
		return std::nullopt;
	found = true;
	std::vector<char> bytes;
	archive.extract(*entry, bytes);
	return parse_mbs_sections(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()));
}

MbsCatalog::Entry& MbsCatalog::ensure(const std::string& mbs) const {
	if (auto found = _cache.find(mbs); found != _cache.end())
		return found->second;

	Entry entry;
	if (!mbs.empty()) {
		try {
			if (auto sections = read_sections(mbs, entry.found)) {
				const auto analyzed = analyze_tracks(*sections);
				entry.tracks.reserve(analyzed.size());
				for (size_t i = 0; i < analyzed.size(); ++i) {
					const SpriteData::Track& track = analyzed[i];
					std::string continuation_name;
					if (track.next_track >= 0 && static_cast<size_t>(track.next_track) < analyzed.size())
						continuation_name = analyzed[static_cast<size_t>(track.next_track)].name;
					entry.tracks.push_back(Track{
						track.name,
						track_duration_frames(*sections, track),
						static_cast<uint32_t>(track.runs.size()),
						!track.disabled && !track.runs.empty(),
						track.one_shot,
						track.next_track,
						std::move(continuation_name),
						track.transition_mode});
				}
				entry.sections = std::move(sections);
				entry.parsed = true;
			}
		} catch (...) {
			entry.parsed = false;
		}
	}
	return _cache.emplace(mbs, std::move(entry)).first->second;
}

const MbsCatalog::Entry& MbsCatalog::get(const std::string& mbs) const { return ensure(mbs); }

std::optional<mbs::v77> MbsCatalog::take_sections(const std::string& mbs) const {
	Entry& entry = ensure(mbs);
	if (!entry.parsed)
		return std::nullopt;
	if (!entry.sections) {
		bool found = entry.found;
		try {
			entry.sections = read_sections(mbs, found);
		} catch (...) {
			return std::nullopt;
		}
		if (!entry.sections)
			return std::nullopt;
	}
	auto sections = std::move(entry.sections);
	entry.sections.reset();
	return sections;
}

bool MbsCatalog::ftx_exists(const std::string& ftx, const std::string& mbs) const {
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
