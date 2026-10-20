#pragma once

#include "asset_loader.hpp"
#include "asset_library.hpp"
#include "playback_diagnostics.hpp"
#include "playback_runtime.hpp"
#include "scene_view.hpp"
#include "viewer_config.hpp"

#include <cstdint>
#include <memory>

class DramaPlayback {
public:
	DramaPlayback(const AssetLoader& loader, ViewerConfig config);
	~DramaPlayback();

	bool handle_inputs();
	void update(float dt);
	void render();
	void capture();

	void request_quit() { _quit = true; }

	bool is_done() const;
	bool probes_done() const;
	bool should_quit() const { return _quit; }
	bool captured() const;
	bool captures_done() const;

private:
	void step();

	const AssetLoader& _loader;
	ViewerConfig _cfg;
	AssetLibrary _library;
	std::unique_ptr<PlaybackDiagnostics> _diagnostics;
	std::unique_ptr<PlaybackRuntime> _runtime;
	std::unique_ptr<SceneView> _view;

	long _frame = 0;
	double _accum = 0.0;
	uint32_t _render_count = 0;
	bool _stepped = false;
	bool _quit = false;
	bool _started = false;
};
