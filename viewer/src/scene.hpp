#pragma once

#include "asset_loader.hpp"
#include "battle_state.hpp"
#include "debug_ui.hpp"
#include "engine/Engine.hpp"
#include "menu_state.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class Scene final : public uvw::BaseGame {
public:
	Scene(std::filesystem::path cpkpath);

	bool handle_inputs() override;
	void render() override;
	void update(float dt) override;

private:
	AssetLoader _loader;
	DebugUI _ui;

	enum class Mode { Menu, Battle };
	Mode _mode = Mode::Menu;

	std::vector<std::string> _bg_names;
	std::unique_ptr<MenuState> _menu;
	std::unique_ptr<BattleState> _battle;

	void start_battle(const MenuSelection& selection);
};
