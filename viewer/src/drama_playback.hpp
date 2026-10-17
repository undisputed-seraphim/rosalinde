#pragma once

#include "asset_loader.hpp"
#include "camera.hpp"
#include "effect_manager.hpp"
#include "scene_renderer.hpp"
#include "screen_fade.hpp"

#include <scarlett/host.hpp>
#include <scarlett/runtime.hpp>

#include <cstdint>
#include <memory>
#include <string>

struct DramaPlaybackConfig {
	std::string entry;
	std::string func;
	std::string chara_table;
	std::string screenshot;
	uint32_t frames = 1;
	long scene_frame = -1;
	float fps = 60.0f;
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
	bool should_quit() const { return _quit; }
	bool captured() const { return _captured; }

private:
	void step();
	void apply_camera();
	void draw_text();

	const AssetLoader& _loader;
	DramaPlaybackConfig _cfg;
	asb::Host _host;
	std::unique_ptr<asb::RuntimeSession> _session;
	SceneRenderer _renderer;
	EffectManager _effects;
	Camera _camera;
	glm::mat4 _projection;
	ScreenFadeOverlay _fade;

	long _frame = 0;
	double _accum = 0.0;
	uint32_t _render_count = 0;
	bool _stepped = false;
	bool _captured = false;
	bool _quit = false;
	bool _started = false;
};
