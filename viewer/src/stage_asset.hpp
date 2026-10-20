#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct StageMpdLayer {
	std::string mpd;
	std::string ftx;
	bool far = false;
};

struct StageLayer {
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

struct StageComposition {
	std::string mbs;
	std::string ftx;
	std::vector<StageMpdLayer> mpd_layers;
	std::vector<StageLayer> layers;

	bool has_base() const { return !mbs.empty() || !mpd_layers.empty(); }
};
