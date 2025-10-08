#pragma once

#include <array>
#include <string_view>

namespace AFS2 {

struct Entry {
	uint32_t id;
	uint32_t offset;
	uint32_t size;
};

struct AFS2Traits {
	static constexpr std::array<std::string_view, 19> Fields = {
		"CueId",
		"ReferenceIndex",
		"AisacControlMap",
		"Length",
		"NumAisacControlMaps",
		"NumRelatedWaveforms",
		"CueName",
		"CueIndex",
		"NumTracks",
		"TrackIndex",
		"CommandIndex",
		"LocalAisacs",
		"GlobalAisacStartIndex",
		"GlobalAisacNumRefs",
		"ParameterPallet",
		"ActionTrackStartIndex",
		"NumActionTracks",
		"Type",
		"ControlWorkArea1"
	};
};


} // namespace AFS2
