#pragma once

#include <array>
#include <criware/utf.hpp>

struct TopLevelAcbTraits {
	static constexpr auto OtherFields = std::to_array<std::string_view>(
		{"AcbGuid",
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
		 "CuePriorityType",
		 "CueTable",
		 "EventTable",
		 "FileIdentifier",
		 "GlobalAisacReferenceTable",
		 "GraphTable",
		 "InstrumentPluginParameterTable",
		 "InstrumentPluginTrackTable",
		 "LipsMorphTable",
		 "MIDITrackTable",
		 "Name",
		 "NumCueLimit",
		 "NumCueLimitListWorks",
		 "NumCueLimitNodeWorks",
		 "OutsideLinkTable",
		 "PaddingArea",
		 "ParameterAction",
		 "ParameterActionCondition",
		 "ProjectKey",
		 "R0",
		 "R1",
		 "R10",
		 "R11",
		 "R12",
		 "R13",
		 "R14",
		 "R15",
		 "R16",
		 "R17",
		 "R18",
		 "R19",
		 "R2",
		 "R20",
		 "R21",
		 "R22",
		 "R23",
		 "R24",
		 "R25",
		 "R26",
		 "R27",
		 "R28",
		 "R29",
		 "R3",
		 "R30",
		 "R31",
		 "R4",
		 "R5",
		 "R6",
		 "R7",
		 "R8",
		 "R9",
		 "SeqCommandTable",
		 "SeqParameterPalletTable",
		 "SequenceTable",
		 "Size",
		 "SoundGeneratorTable",
		 "SoundInstruments",
		 "SoundProgramBankCommandTable",
		 "SoundProgramBankKey",
		 "StopAction",
		 "StreamAwbAfs2Header",
		 "StreamAwbHash",
		 "StreamAwbTocWork",
		 "StreamAwbTocWorkOld",
		 "StreamAwbTocWork_Old",
		 "StringValueTable",
		 "SynthCommandTable",
		 "SynthParameterPalletTable",
		 "SynthTable",
		 "Target",
		 "TrackCommandTable",
		 "TrackEventTable",
		 "TrackParameterPalletTable",
		 "TrackTable",
		 "Type",
		 "Version",
		 "VersionString",
		 "WaveformExtensionDataTable",
		 "WaveformTable"});
};

struct ACBTraits {
	static constexpr auto Fields = std::to_array<std::string_view>({"CueTable", "WaveformTable", "SynthTable"});
	using Entry = std::tuple<UTF::field::data_t, UTF::field::data_t, UTF::field::data_t>;
};

class ACB : public UTFTable<ACBTraits> {
public:
	using Base = UTFTable<ACBTraits>;

	using Base::operator<<;
	using Base::operator>>;
	using Base::entry_tuple;
	using Base::Fields;
	using Base::NumFields;

	ACB();
	ACB(const ACB&) = delete;

	friend std::istream& operator>>(std::istream&, ACB&);

private:
	struct CueRecord {
		uint32_t cue_id;
		uint8_t reference_type;
		uint8_t reference_index;

		uint16_t waveform_index;
		uint16_t waveform_id;
		uint8_t encode_type;

		static constexpr int cue_name_max_length = 256;
		char cue_name[cue_name_max_length];
	};
	std::vector<CueRecord> _cueTable;
};

struct CueNameTableTraits {
	static constexpr auto Fields = std::to_array<std::string_view>({"CueIndex", "CueName"});
	using Entry = std::tuple<unsigned, std::string>;
};

struct TrackTableTraits {
	static constexpr auto Fields =
		std::to_array<std::string_view>({"CommandIndex", "EventIndex", "GlobalAisacNumRefs", "GlobalAisacStartIndex"});
	using Entry = std::tuple<uint16_t, uint8_t, uint16_t, uint16_t>;
};
