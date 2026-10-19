#pragma once

#include <string>
#include <vector>

struct StageLayer {
	std::string mpd;
	std::string ftx;
	bool far = false;
};

struct StageBackground {
	std::string mbs;
	std::string ftx;
	std::vector<StageLayer> layers;
};
