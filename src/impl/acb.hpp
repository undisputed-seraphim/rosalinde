#pragma once

#include "utf.hpp"

struct ACBTraits {
	static constexpr std::array<std::string_view, 3> Fields = {"CueTable", "WaveformTable", "SynthTable"};
	using Entry = std::tuple<uint64_t, uint64_t, uint64_t>;
};

class ACB : public UTFTable<ACBTraits> {
public:
	using Base = UTFTable<ACBTraits>;
	using Base::Base;

	using Base::operator<<;
	using Base::operator>>;
};

struct TopLevelAcbTraits {
	static constexpr std::array<std::string_view, 18> OtherFields = {
		"AcbGuid",
		"AcbVolume",
		"AcfMd5Hash",
		"AcfReferenceTable",
		"ActionTrackTable",
		"AisacControlNameTable",
		"AisacNameTable",
		"AisacTable",
		"AutoModulationTable",
		"AwbFile",
		"BeatSyncInfoTable",
		"BlockSequenceTable",
		"BlockTable",
		"CategoryExtension",
		"CharacterEncodingType",
		"CueLimitWorkTable",
		"CueNameTable",
		"CuePriorityType"};
};