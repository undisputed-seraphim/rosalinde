#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct StageLayer {
	std::string mpd;
	std::string ftx;
	bool far = false;
};

struct StageObject {
	std::string mbs;
	std::string ftx;
	int track_index = -1;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float scale = 1.0f;
	int alpha = 255;
	int order = 0;
};

struct StageBackground {
	std::string mbs;
	std::string ftx;
	std::vector<StageLayer> layers;
	std::vector<StageObject> objects;
};
