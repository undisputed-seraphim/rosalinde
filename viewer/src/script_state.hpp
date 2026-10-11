#pragma once

#include "asset_loader.hpp"
#include "camera.hpp"
#include "debug_ui.hpp"
#include "sprite_layer.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

struct ScriptSceneConfig {
	std::string entry; // e.g. "Script/Dr_OW_0_Opening.asb"
	std::string func;  // scene function name (optional; default: scene00/MAIN)
	int limit = 8;     // max sprite entities to load
	std::string camera = "fit"; // fit | first | last | <index>
	std::string chara_table;    // path to a generated character name -> asset table
	std::string screenshot;
	uint32_t frames = 1;
};

class ScriptState {
public:
	ScriptState(const AssetLoader& loader, DebugUI& ui, ScriptSceneConfig cfg);

	bool handle_inputs();
	void update(float dt);
	void render();

	bool is_done() const { return _done; }
	bool should_quit() const { return _quit; }
	bool captured() const { return _captured; }

private:
	const AssetLoader& _loader;
	DebugUI& _ui;
	Camera _camera;
	glm::mat4 _projection;

	std::vector<std::unique_ptr<SpriteLayer>> _layers;
	ScriptSceneConfig _cfg;

	uint32_t _frame = 0;
	bool _captured = false;
	bool _done = false;
	bool _quit = false;
};
