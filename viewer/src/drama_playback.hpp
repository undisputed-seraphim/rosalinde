#pragma once

#include "asset_loader.hpp"
#include "camera.hpp"
#include "effect_manager.hpp"
#include "probe.hpp"
#include "runtime_data.hpp"
#include "scene_renderer.hpp"
#include "screen_fade.hpp"
#include "trace_writer.hpp"

#include <scarlett/host.hpp>
#include <scarlett/runtime.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

struct DramaPlaybackConfig {
	std::string entry;
	std::string func;
	std::string chara_table;
	std::string data_dir;
	std::string screenshot;
	uint32_t frames = 1;
	long scene_frame = -1;
	float fps = 60.0f;
	std::string trace;
	std::string trace_categories;
	std::vector<long> capture_frames;
	std::string capture_dir;
	std::string capture_prefix = "frame";
	std::vector<std::string> only;
	std::vector<std::string> hide;
	bool keep_stage = false;
	std::string probe_dir;
	std::vector<long> probe_frames;
};

class DramaPlayback {
public:
	DramaPlayback(const AssetLoader& loader, DramaPlaybackConfig cfg);
	~DramaPlayback();

	bool handle_inputs();
	void update(float dt);
	void render();
	void capture();

	void request_quit() { _quit = true; }

	bool is_done() const { return _session && _session->finished(); }
	bool probes_done() const { return _probe.done(); }
	bool should_quit() const { return _quit; }
	bool captured() const { return _captured; }
	bool captures_done() const { return _captures_done; }

private:
	void step();
	void apply_camera();
	void sync_stage();
	void draw_text();
	bool write_screenshot(const std::string& path);

	const AssetLoader& _loader;
	DramaPlaybackConfig _cfg;
	ViewerRuntimeData _runtime;
	asb::Host _host;
	std::unique_ptr<asb::RuntimeSession> _session;
	SceneRenderer _renderer;
	EffectManager _effects;
	Camera _camera;
	glm::mat4 _projection;
	ScreenFadeOverlay _fade;
	TraceWriter _trace;
	ProbeWriter _probe;

	long _frame = 0;
	double _accum = 0.0;
	uint32_t _render_count = 0;
	bool _stepped = false;
	bool _captured = false;
	bool _captures_done = false;
	bool _capture_mode = false;
	size_t _capture_index = 0;
	std::unordered_set<long> _captured_frames;
	bool _probe_mode = false;
	size_t _probe_index = 0;
	bool _quit = false;
	int _stage_type = -1;
	bool _started = false;
};
