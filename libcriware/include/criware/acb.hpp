#pragma once

#include <criware/utf.hpp>

using ACBSchema = decltype(schema::make(
	schema::col<"CueTable", UTF::field::data_t>(),
	schema::col<"WaveformTable", UTF::field::data_t>(),
	schema::col<"SynthTable", UTF::field::data_t>()));

class ACB {
public:
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
	table<ACBSchema> _table;
	std::vector<CueRecord> _cueTable;
};
