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
	float scale_y = 0.0f; // BBS Y scale; 0 = uniform with `scale`
	int alpha = 255;
	int order = 0;
	bool additive = false; // BBS material selects additive blending for effect keyframes
	// BBS object flags bits 24..31: one bit per DrCreateStage weather type.
	// Zero means un-gated.
	uint8_t weather_mask = 0;
};

struct StageComposition {
	std::string mbs;
	std::string ftx;
	std::vector<StageMpdLayer> mpd_layers;
	std::vector<StageLayer> layers;
	// DrCreateStage weather type, used to test each BBS object's weather mask.
	int weather_type = 0;
	// Atmosphere/post-effect strength from the BBS preset (weather_record::cloud).
	// This is not a rain gate; rain/snow layers are selected by object weather_mask.
	float weather_amount = 0.0f;
	float weather_fog = 0.0f;

	bool has_base() const { return !mbs.empty() || !mpd_layers.empty(); }
};
