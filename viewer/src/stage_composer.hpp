#pragma once

#include "stage_asset.hpp"

class StageComposer {
public:
	void seed(StageComposition composition);
	void apply(StageComposition composition);
	void clear();
	const StageComposition& current() const { return _current; }

private:
	StageComposition _current;
	std::vector<StageLayer> _base_layers;
};
