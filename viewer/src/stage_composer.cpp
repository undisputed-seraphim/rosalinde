#include "stage_composer.hpp"

#include <utility>

void StageComposer::seed(StageComposition composition) {
	_current = std::move(composition);
	_base_layers = _current.layers;
	// An explicit seed is the session base. UcStStage +0xab0 is object/scene
	// data and does not classify BBS layers as base versus effect.
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

	// A seeded base is retained and incoming BBS layers are composed on top.
	// Confirmed opening case: stage 179 / Opening_00 is an effect-only overlay
	// over stage 180 / Opening_010. BBS replace/overlay classification still
	// needs an engine-side composition rule.
	_current.layers = _base_layers;
	_current.layers.insert(_current.layers.end(), composition.layers.begin(), composition.layers.end());
}

void StageComposer::clear() {
	_current = StageComposition{};
	_base_layers.clear();
}
