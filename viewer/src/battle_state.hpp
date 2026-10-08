#pragma once

#include "animation_graph.hpp"
#include "asset_loader.hpp"
#include "camera.hpp"
#include "debug_ui.hpp"
#include "menu_state.hpp"
#include "sprite_layer.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

class BattleState {
public:
	BattleState(const AssetLoader& loader, DebugUI& ui, const MenuSelection& selection);

	bool handle_inputs();
	void update(float dt);
	void render();

	bool is_done() const { return _done; }
	bool should_quit() const { return _quit; }

private:
	const AssetLoader& _loader;
	DebugUI& _ui;
	Camera _camera;
	glm::mat4 _projection;

	std::vector<std::unique_ptr<SpriteLayer>> _layers;
	size_t _active_layer = 0;

	AnimationGraph _graph;

	int _variant_side = 0;
	bool _done = false;
	bool _quit = false;

	std::string _screenshot_path;
	bool _captured = false;

	void load_characters(const MenuSelection& selection);
	void apply_action(AnimationGraph::Action action);
};
