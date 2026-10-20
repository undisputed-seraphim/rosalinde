#include "stage_composer.hpp"

#include <utility>

void StageComposer::seed(StageComposition composition) {
	_current = std::move(composition);
	_base_layers = _current.layers;
	// TODO(stage): UcStStage +0xab0 (stage_data) should identify whether this
	// seeded scene is a base or a transient effect layer. Until that record is
	// decoded, an explicitly seeded composition is treated as the session base.
}

void StageComposer::apply(StageComposition composition) {
	if (composition.has_base()) {
		_current.mbs = std::move(composition.mbs);
		_current.ftx = std::move(composition.ftx);
		_current.mpd_layers = std::move(composition.mpd_layers);
	}

	if (_base_layers.empty()) {
		_current.layers = std::move(composition.layers);
		return;
	}

	// TODO(stage): UcStStage +0xab0 (stage_data) should identify whether this
	// BBS replaces the seeded base or only contributes effect layers. Until that
	// record is decoded, a seeded base is retained and incoming BBS layers are
	// composed on top of it. Confirmed opening case: stage 179 / Opening_00 is
	// an effect-only overlay over stage 180 / Opening_010.
	_current.layers = _base_layers;
	_current.layers.insert(_current.layers.end(), composition.layers.begin(), composition.layers.end());
}

void StageComposer::clear() {
	_current = StageComposition{};
	_base_layers.clear();
}
