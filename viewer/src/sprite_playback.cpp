#include "sprite_playback.hpp"

#include "background_tracks.hpp"

#include <algorithm>
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
	bool hold_finished,
	bool character) {
	asset = std::move(next_asset);
	instances.clear();
	track_index = -1;
	_policy = policy;
	if (!asset)
		return;

	const uint32_t attribute_mask = state.appearance_mask;
	const bool mone_kind = state.kind.rfind("CreateSpriteMone", 0) == 0;
	auto stateful_mone = [&](int track) {
		return mone_kind && track >= 0 && static_cast<size_t>(track) < asset->data.tracks.size() &&
			   asset->data.tracks[track].sa_main_sequence > 0;
	};
	if (policy == TrackPolicy::AllEnabledTracks) {
		for (uint32_t i : select_background_tracks(asset->data.v77)) {
			if (!track_usable(asset->data, static_cast<int>(i)))
				continue;
			instances.emplace_back(&asset->data, i, attribute_mask);
			instances.back().playback.hold_finished = hold_finished;
			instances.back().background = true;
			instances.back().character = character;
			instances.back().mone = stateful_mone(static_cast<int>(i));
			instances.back().current_sequence = state.sequence_index;
			instances.back().flip_v = flip_v;
			instances.back().play(i);
		}
	} else {
		const int track = choose_track(asset->data, state);
		if (track < 0)
			return;
		track_index = track;
		instances.emplace_back(&asset->data, static_cast<uint32_t>(track), attribute_mask);
		instances.back().playback.hold_finished = hold_finished;
		instances.back().character = character;
		instances.back().mone = stateful_mone(track);
		instances.back().current_sequence = state.sequence_index;
		instances.back().flip_v = flip_v;
		instances.back().play(static_cast<uint32_t>(track));
	}
	applied_start_frame = state.animation_start_frame;
	apply(state);
}

void SpritePlayback::apply(const asb::RuntimeEntity& state) {
	if (_policy == TrackPolicy::ScriptTrack && asset && !instances.empty()) {
		const int track = choose_track(asset->data, state);
		// Re-selecting the same track is meaningful: DrUiSetAnimation /
		// DrAnimation can restart the current animation from its first frame.
		const bool restart = state.animation_start_frame != applied_start_frame;
		if (track >= 0 && (track != track_index || restart)) {
			track_index = track;
			instances[0].play(static_cast<uint32_t>(track));
		}
		applied_start_frame = state.animation_start_frame;
	}
	for (auto& instance : instances) {
		instance.playback.paused = state.paused || state.anim_paused;
		if (instance.current_sequence != state.sequence_index) {
			instance.current_sequence = state.sequence_index;
			if (_policy == TrackPolicy::ScriptTrack)
				instance.play(instance.playback.track_idx);
		}
	}
}

void SpritePlayback::apply_track_order(const std::vector<uint32_t>& order) {
	if (order.empty())
		return;
	auto rank = [&](uint32_t track) {
		const auto it = std::find(order.begin(), order.end(), track);
		return it == order.end() ? order.size() : static_cast<size_t>(it - order.begin());
	};
	std::stable_sort(instances.begin(), instances.end(), [&](const SpriteInstance& a, const SpriteInstance& b) {
		return rank(a.playback.track_idx) < rank(b.playback.track_idx);
	});
}

void SpritePlayback::advance(float dt) {
	for (auto& instance : instances)
		instance.advance(dt * rate);
}
