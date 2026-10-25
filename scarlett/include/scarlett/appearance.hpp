#pragma once

#include <cstdint>

namespace asb {

// Layer selection and colour preset resolved from the CharaSet tables. Kept in
// one struct so the CharaSet record and the live runtime entity cannot drift
// when a column is added or renamed.
struct CharaAppearance {
	uint32_t variant = 0xFFFFFFFFu; // CharaSet variant; 0xFFFFFFFF = no selector
	uint32_t mask = 0;				// suppressed s4 attribute bits from chara_model.mask
	uint32_t preset = 0;			// chara_color_table row
	uint32_t palette[4] = {};		// preset colour0..colour3
	uint32_t color_mask = 0;		// enabled s4.color_id bits
};

} // namespace asb
