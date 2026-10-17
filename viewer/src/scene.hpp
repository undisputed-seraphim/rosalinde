#pragma once

#include "asset_loader.hpp"
#include "battle_state.hpp"
#include "debug_ui.hpp"
#include "drama_playback.hpp"
#include "engine/Engine.hpp"
#include "menu_state.hpp"
#include "runtime_state.hpp"
#include "script_state.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct ViewerConfig {
	std::filesystem::path cpk;
	std::string screenshot;
	uint32_t frames = 1;
	std::string left_class = "ALBION_PRINCESS_HG_F";
	std::string right_class = "CORNIA_PRINCESS_F";
	std::string background = "Classpedia_BG";
	std::string script;		  // Script/X.asb — render a script-composed scene
	std::string scene_func;	  // scene function name (default: scene00/MAIN)
	int scene_limit = 0;	  // max sprite entities; 0 = per-mode default (8 / 256)
	std::string scene_camera; // empty = per-mode default (fit static / design runtime)
	std::string chara_table;  // generated character name -> asset table
	bool runtime = false;	  // run the hosted VM instead of the static fold
	long scene_frame = -1;	  // runtime: scene frame to advance to (-1 = real time)
	float scene_fps = 60.0f;  // runtime: frame step rate (the cinematic is 60fps)
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

	enum class Mode { Menu, Battle, Script, Runtime, Drama };
	Mode _mode = Mode::Menu;

	std::vector<std::string> _bg_names;
	std::unique_ptr<MenuState> _menu;
	std::unique_ptr<BattleState> _battle;
	std::unique_ptr<ScriptState> _script;
	std::unique_ptr<RuntimeState> _runtime;
	std::unique_ptr<DramaPlayback> _drama;

	void start_battle(const MenuSelection& selection);
};
