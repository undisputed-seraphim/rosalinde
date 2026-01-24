#include <istream>

#include <criware/acb.hpp>
#include <criware/substream.hpp>

struct CueTableTraits {
	static constexpr auto Fields = std::to_array<std::string_view>(
		{"AisacControlMap", "CueId", "Length", "NumAisacControlMaps", "NumRelatedWaveforms", "ReferenceIndex"});
	using Entry = std::tuple<std::string, uint8_t, uint32_t, uint8_t, uint8_t, uint8_t>;
};
using CueTable = UTFTable<CueTableTraits>;

struct WaveformTableTraits {
	static constexpr auto Fields = std::to_array<std::string_view>(
		{"EncodeType", "ExtensionData", "LoopFlag", "MemoryAwbId", "NumSamples", "StreamAwbId", "Streaming"});
	using Entry = std::tuple<uint16_t, uint16_t, uint8_t, uint16_t, uint32_t, uint8_t, uint8_t>;
};
using WaveformTable = UTFTable<WaveformTableTraits>;

struct SynthTableTraits {
	static constexpr auto Fields =
		std::to_array<std::string_view>({"CommandIndex", "ControlWorkArea1", "ControlWorkArea2", "ReferenceItems"});
	using Entry = std::tuple<uint16_t, uint8_t, uint8_t, UTF::field::data_t>;
};
using SynthTable = UTFTable<SynthTableTraits>;

ACB::ACB()
	: UTFTable<ACBTraits>() {}

std::istream& operator>>(std::istream& is, ACB& acb) {
	auto it = acb.begin();

	CueTable cueTable;
	auto ssbuf = substreambuf(is.rdbuf(), std::get<0>(*it).offset, std::get<0>(*it).size);
	std::istream(&ssbuf) >> cueTable;

	WaveformTable waveformTable;
	ssbuf = substreambuf(is.rdbuf(), std::get<1>(*it).offset, std::get<1>(*it).size);
	std::istream(&ssbuf) >> waveformTable;

	SynthTable synthTable;
	ssbuf = substreambuf(is.rdbuf(), std::get<2>(*it).offset, std::get<2>(*it).size);
	std::istream(&ssbuf) >> synthTable;

	for (uint32_t i = 0; i < cueTable.size(); ++i) {
		auto [acm, cueid, length, nracm, nrrelwavf, refidx] = cueTable.at(i);

		// TODO
		int refType = 0;

		UTF::field::data_t refItem;
		switch (refType) {
		case 2: {
			auto [cmdIdx, ctrlWrkA1, ctrlWrkA2, refItems] = synthTable.at(refidx);
			refItem = refItems;
			break;
		}
		case 3:
		case 8: {
			if (i == 0) {
				auto [cmdIdx, ctrlWrkA1, ctrlWrkA2, refItems] = synthTable.at(0);
				refItem = refItems;

			} else {
			}
			break;
		}
		default: {
			throw std::runtime_error("Unsupported ReferenceType");
		}
		}
	}

	return is;
}