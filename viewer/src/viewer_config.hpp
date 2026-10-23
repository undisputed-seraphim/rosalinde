#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct ViewerConfig {
	std::filesystem::path cpk;
	std::string screenshot;
	uint32_t frames = 1;
	std::string script;		 // Script/X.asb — required drama module
	std::string scene_func;	 // scene function name (default: auto)
	std::string chara_table; // generated character name -> asset table
	std::string data_dir;	 // runtime data directory (default: viewer/data/generated or installed share/rosalinde)
	int initial_stage = -1;	 // stage type whose composition seeds the session
	long scene_frame = -1;	 // scene frame to advance to (-1 = real time)
	bool headless = false;	 // run without a window/GL context
	long run_to = -1;		 // headless: stop after this scene frame (-1 = script end)
	long max_frames = 200000; // headless: safety cap on scene frames
	std::string state;		 // headless: write a scene state snapshot (text)
	std::string state_json;	 // headless: write the full scene state JSON
	float scene_fps = 60.0f; // playback frame step rate
	std::string trace;		 // optional NDJSON event trace
	std::string trace_categories;
	std::vector<long> trace_frames;
	std::vector<std::string> trace_objects;
	int trace_limit = 0;
	std::string calls;		 // optional native-call census text
	std::string calls_json;	 // optional native-call census JSON
	std::string record;		 // optional compact runtime timeline NDJSON
	long record_every = 1;
	std::vector<long> capture_frames;
	std::string capture_dir;
	std::string capture_prefix = "frame";
	std::vector<std::string> only;
	std::vector<std::string> hide;
	bool keep_stage = false;
	std::string probe_dir;
	std::vector<long> probe_frames;
	std::array<float, 4> clear_color = {0.45f, 0.55f, 0.60f, 1.0f};
};
