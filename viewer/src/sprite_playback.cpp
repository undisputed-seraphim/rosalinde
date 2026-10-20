#include "sprite_playback.hpp"

#include "background_tracks.hpp"

#include <utility>

bool SpritePlayback::track_usable(const SpriteData& data, int index) {
	return index >= 0 && static_cast<size_t>(index) < data.tracks.size() && !data.tracks[index].disabled &&
		   !data.tracks[index].runs.empty();
}

int SpritePlayback::choose_track(const SpriteData& data, const asb::RuntimeEntity& state) {
	if (track_usable(data, state.track_index))
		return state.track_index;
	if (!state.track.empty()) {
		if (const auto* track = data.find_track(state.track))
			return static_cast<int>(track - data.tracks.data());
	}
	for (size_t i = 0; i < data.tracks.size(); ++i)
		if (track_usable(data, static_cast<int>(i)))
			return static_cast<int>(i);
	return -1;
}

void SpritePlayback::reset(
	SpriteAssetPtr next_asset,
	TrackPolicy policy,
	const asb::RuntimeEntity& state,
	bool flip_v,
	bool hold_finished) {
	asset = std::move(next_asset);
	instances.clear();
	track_index = -1;
	_policy = policy;
	if (!asset)
		return;

	if (policy == TrackPolicy::AllEnabledTracks) {
		for (uint32_t i : select_background_tracks(asset->data.v77)) {
			if (!track_usable(asset->data, static_cast<int>(i)))
				continue;
			instances.emplace_back(&asset->data, i, 0xFFFFFFFFu);
			instances.back().hold_finished = hold_finished;
			instances.back().flip_v = flip_v;
			instances.back().play(i);
		}
	} else {
		const int track = choose_track(asset->data, state);
		if (track < 0)
			return;
		track_index = track;
		instances.emplace_back(&asset->data, static_cast<uint32_t>(track), 0xFFFFFFFFu);
		instances.back().flip_v = flip_v;
		instances.back().play(static_cast<uint32_t>(track));
	}
	apply(state);
}

void SpritePlayback::apply(const asb::RuntimeEntity& state) {
	if (_policy == TrackPolicy::ScriptTrack && asset && !instances.empty()) {
		const int track = choose_track(asset->data, state);
		if (track >= 0 && track != track_index) {
			track_index = track;
			instances[0].play(static_cast<uint32_t>(track));
		}
	}
	for (auto& instance : instances)
		instance.paused = state.paused || state.anim_paused;
}

void SpritePlayback::advance(float dt) {
	for (auto& instance : instances)
		instance.advance(dt * rate);
}
