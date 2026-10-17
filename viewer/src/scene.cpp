#include "scene.hpp"
#include "tables.hpp"
#include "trace.hpp"

#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <glxx/error.hpp>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <iostream>
#include <stdexcept>

Scene::Scene(ViewerConfig config)
	: _config(std::move(config))
	, _loader(_config.cpk) {

	viewer::trace::stamp("Scene ctor: AssetLoader ready");
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(&message_callback, NULL);

	printf(" Version: %s\n", glGetString(GL_VERSION));
	printf("  Vendor: %s\n", glGetString(GL_VENDOR));
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf(" Shading: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	for (const auto& [name, _] : BattleBGs)
		_bg_names.push_back(name);

	if (!_config.script.empty()) {
		ImGui::GetIO().IniFilename = nullptr;
		DramaPlaybackConfig dc;
		dc.entry = _config.script;
		dc.func = _config.scene_func;
		dc.chara_table = _config.chara_table;
		dc.screenshot = _config.screenshot;
		dc.frames = _config.frames;
		dc.scene_frame = _config.scene_frame;
		dc.fps = _config.scene_fps;
		_drama = std::make_unique<DramaPlayback>(_loader, std::move(dc));
		_mode = Mode::Drama;
		viewer::trace::stamp("Scene ctor: DramaPlayback ready");
	} else if (!_config.screenshot.empty()) {
		ImGui::GetIO().IniFilename = nullptr;
		MenuSelection selection;
		selection.left_class = _config.left_class;
		selection.right_class = _config.right_class;
		selection.background = _config.background;
		start_battle(selection);
	} else {
		_menu = std::make_unique<MenuState>(_loader.class_names(), _bg_names);
	}
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
				if (!_config.screenshot.empty())
					return true;
				_battle.reset();
				_mode = Mode::Menu;
				_menu = std::make_unique<MenuState>(_loader.class_names(), _bg_names);
			}
		}

		if (_mode == Mode::Script && _script) {
			_script->handle_inputs();
			if (_script->should_quit())
				return true;
		}

		if (_mode == Mode::Runtime && _runtime) {
			_runtime->handle_inputs();
			if (_runtime->should_quit() || _runtime->is_done())
				return true;
		}

		if (_mode == Mode::Drama && _drama) {
			if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
				_drama->request_quit();
			if (_drama->should_quit())
				return true;
		}
	}
	if (!_config.screenshot.empty() && _battle && _battle->captured())
		return true;
	if (!_config.screenshot.empty() && _script && _script->captured())
		return true;
	if (!_config.screenshot.empty() && _runtime && _runtime->captured())
		return true;
	if (!_config.screenshot.empty() && _drama && _drama->captured())
		return true;
	if (_mode == Mode::Runtime && _runtime && (_runtime->is_done() || _runtime->should_quit()))
		return true;
	if (_mode == Mode::Drama && _drama && _drama->should_quit())
		return true;
	return false;
}

void Scene::render() {
	static bool traced = false;
	if (!traced) {
		traced = true;
		viewer::trace::stamp("first frame: Scene::render()");
	}
	_ui.begin_frame();

	if (_mode == Mode::Menu) {
		MenuSelection selection;
		if (_menu->draw(selection)) {
			start_battle(selection);
		}
	} else if (_mode == Mode::Battle && _battle) {
		_battle->render();
	} else if (_mode == Mode::Script && _script) {
		_script->render();
	} else if (_mode == Mode::Runtime && _runtime) {
		_runtime->render();
	} else if (_mode == Mode::Drama && _drama) {
		_drama->render();
	}

	_ui.end_frame();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	if (_mode == Mode::Drama && _drama)
		_drama->capture();
}

void Scene::update(float dt) {
	if (_mode == Mode::Battle && _battle) {
		_battle->update(dt);
	} else if (_mode == Mode::Script && _script) {
		_script->update(dt);
	} else if (_mode == Mode::Runtime && _runtime) {
		_runtime->update(dt);
	} else if (_mode == Mode::Drama && _drama) {
		_drama->update(dt);
	}
}

void Scene::start_battle(const MenuSelection& selection) {
	_battle = std::make_unique<BattleState>(_loader, _ui, selection, _config.screenshot, _config.frames);
	_mode = Mode::Battle;
	_menu.reset();
}
