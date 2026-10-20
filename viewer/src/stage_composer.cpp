#include "stage_composer.hpp"

#include <utility>

void StageComposer::apply(StageComposition composition) {
	if (composition.has_base()) {
		_current.mbs = std::move(composition.mbs);
		_current.ftx = std::move(composition.ftx);
		_current.mpd_layers = std::move(composition.mpd_layers);
	}
	_current.layers = std::move(composition.layers);
	// TODO(stage): UcStStage +0xab0 (stage_data) should identify whether this
	// BBS replaces the active base or only contributes effect layers. Until that
	// record is decoded, only explicit base compositions replace the base.
	// Confirmed opening case: opening stage 179 / Opening_00 is an effect-only
	// overlay; the forest base is stage 180 / Opening_010, loaded by the title
	// script and retained across the script transition.
}

void StageComposer::clear() {
	_current = StageComposition{};
}
