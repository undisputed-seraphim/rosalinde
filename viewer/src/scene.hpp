#pragma once

#include "asset_loader.hpp"
#include "battle_state.hpp"
#include "debug_ui.hpp"
#include "engine/Engine.hpp"
#include "menu_state.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct ViewerConfig {
	std::filesystem::path cpk;
	std::string screenshot;
	uint32_t frames = 1;
	std::string left_class = "HighPriestess";
	std::string right_class = "Crusader";
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

	enum class Mode { Menu, Battle };
	Mode _mode = Mode::Menu;

	std::vector<std::string> _bg_names;
	std::unique_ptr<MenuState> _menu;
	std::unique_ptr<BattleState> _battle;

	void start_battle(const MenuSelection& selection);
};
