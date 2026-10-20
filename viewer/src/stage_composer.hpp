#pragma once

#include "stage_asset.hpp"

class StageComposer {
public:
	void apply(StageComposition composition);
	void clear();
	const StageComposition& current() const { return _current; }

private:
	StageComposition _current;
};
