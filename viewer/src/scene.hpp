#pragma once

#include "asset_loader.hpp"
#include "debug_ui.hpp"
#include "drama_playback.hpp"
#include "engine/Engine.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct ViewerConfig {
	std::filesystem::path cpk;
	std::string screenshot;
	uint32_t frames = 1;
	std::string script;		 // Script/X.asb — required drama module
	std::string scene_func;	 // scene function name (default: auto)
	std::string chara_table; // generated character name -> asset table
	long scene_frame = -1;	 // scene frame to advance to (-1 = real time)
	float scene_fps = 60.0f; // playback frame step rate
	std::string trace;		 // optional NDJSON event trace
	std::string trace_categories;
	std::vector<long> capture_frames;
	std::string capture_dir;
	std::string capture_prefix = "frame";
};

class Scene final : public uvw::BaseGame {
public:
	Scene(ViewerConfig config);

	bool handle_inputs() override;
	void render() override;
	void update(float dt) override;

private:
	ViewerConfig _config;
	AssetLoader _loader;
	DebugUI _ui;

	std::unique_ptr<DramaPlayback> _drama;
};
