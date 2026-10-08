#include "scene.hpp"
#include "tables.hpp"

#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <glxx/error.hpp>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <iostream>
#include <stdexcept>

Scene::Scene(std::filesystem::path cpkpath)
	: _loader(cpkpath) {

	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(&message_callback, NULL);

	printf(" Version: %s\n", glGetString(GL_VERSION));
	printf("  Vendor: %s\n", glGetString(GL_VENDOR));
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf(" Shading: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	for (const auto& [name, _] : BattleBGs)
		_bg_names.push_back(name);

	_menu = std::make_unique<MenuState>(_loader.class_names(), _bg_names);
}

bool Scene::handle_inputs() {
	SDL_Event event{};
	while (SDL_PollEvent(&event)) {
		ImGui_ImplSDL3_ProcessEvent(&event);

		if (event.type == SDL_EVENT_QUIT)
			return true;
		if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
			return true;

		if (_mode == Mode::Battle && _battle) {
			_battle->handle_inputs();
			if (_battle->should_quit())
				return true;
			if (_battle->is_done()) {
				_battle.reset();
				_mode = Mode::Menu;
				_menu = std::make_unique<MenuState>(_loader.class_names(), _bg_names);
			}
		}
	}
	return false;
}

void Scene::render() {
	_ui.begin_frame();

	if (_mode == Mode::Menu) {
		MenuSelection selection;
		if (_menu->draw(selection)) {
			start_battle(selection);
		}
	} else if (_mode == Mode::Battle && _battle) {
		_battle->render();
	}

	_ui.end_frame();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Scene::update(float dt) {
	if (_mode == Mode::Battle && _battle) {
		_battle->update(dt);
	}
}

void Scene::start_battle(const MenuSelection& selection) {
	_battle = std::make_unique<BattleState>(_loader, _ui, selection);
	_mode = Mode::Battle;
	_menu.reset();
}
