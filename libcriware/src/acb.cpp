#include <criware/acb.hpp>

#include <cassert>
#include <cstring>
#include <istream>
#include <span>
#include <stdexcept>
#include <vector>

using CueTableSchema = decltype(schema::make(
	schema::col<"AisacControlMap", std::string>(),
	schema::col<"CueId", uint8_t>(),
	schema::col<"Length", uint32_t>(),
	schema::col<"NumAisacControlMaps", uint8_t>(),
	schema::col<"NumRelatedWaveforms", uint8_t>(),
	schema::col<"ReferenceIndex", uint8_t>()));
using CueTable = table<CueTableSchema>;

using WaveformTableSchema = decltype(schema::make(
	schema::col<"EncodeType", uint16_t>(),
	schema::col<"ExtensionData", uint16_t>(),
	schema::col<"LoopFlag", uint8_t>(),
	schema::col<"MemoryAwbId", uint16_t>(),
	schema::col<"NumSamples", uint32_t>(),
	schema::col<"StreamAwbId", uint8_t>(),
	schema::col<"Streaming", uint8_t>()));
using WaveformTable = table<WaveformTableSchema>;

using SynthTableSchema = decltype(schema::make(
	schema::col<"CommandIndex", uint16_t>(),
	schema::col<"ControlWorkArea1", uint8_t>(),
	schema::col<"ControlWorkArea2", uint8_t>(),
	schema::col<"ReferenceItems", UTF::field::data_t>()));
using SynthTable = table<SynthTableSchema>;

ACB::ACB() {}

std::istream& operator>>(std::istream& is, ACB& acb) {
	auto pos = is.tellg();
	is.seekg(0, std::ios::end);
	auto sz = static_cast<size_t>(is.tellg() - pos);
	is.seekg(pos, std::ios::beg);
	std::vector<uint8_t> buf(sz);
	is.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(sz));
	auto data = std::span<const uint8_t>(buf);

	acb._table = table<ACBSchema>::parse(data);

	auto& cue = acb._table.column<"CueTable">();
	auto& wave = acb._table.column<"WaveformTable">();
	auto& synth = acb._table.column<"SynthTable">();

	CueTable cueTable = CueTable::parse(data.subspan(cue[0].offset, cue[0].size));
	WaveformTable waveformTable = WaveformTable::parse(data.subspan(wave[0].offset, wave[0].size));
	SynthTable synthTable = SynthTable::parse(data.subspan(synth[0].offset, synth[0].size));

	for (uint32_t i = 0; i < cueTable.size(); ++i) {
		const auto& [acm, cueid, length, nracm, nrrelwavf, refidx] = cueTable[i];

		int refType = 0;

		UTF::field::data_t refItem;
		switch (refType) {
		case 2: {
			const auto& [cmdIdx, ctrlWrkA1, ctrlWrkA2, refItems] = synthTable[refidx];
			refItem = refItems;
			break;
		}
		case 3:
		case 8: {
			if (i == 0) {
				const auto& [cmdIdx, ctrlWrkA1, ctrlWrkA2, refItems] = synthTable[0];
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
