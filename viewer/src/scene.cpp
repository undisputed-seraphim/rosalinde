#include "scene.hpp"
#include "trace.hpp"

#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <glxx/error.hpp>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <stdexcept>

Scene::Scene(ViewerConfig config)
	: _config(std::move(config))
	, _loader(_config.cpk) {

	viewer::trace::stamp("Scene ctor: AssetLoader ready");
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(&message_callback, NULL);

	spdlog::info("GL version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
	spdlog::info("GL vendor: {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
	spdlog::info("GL renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
	spdlog::info("GL shading language: {}", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

	if (_config.script.empty())
		throw std::runtime_error("--script is required");

	ImGui::GetIO().IniFilename = nullptr;
	_drama = std::make_unique<DramaPlayback>(_loader, _config);
	viewer::trace::stamp("Scene ctor: DramaPlayback ready");
}

bool Scene::handle_inputs() {
	SDL_Event event{};
	while (SDL_PollEvent(&event)) {
		ImGui_ImplSDL3_ProcessEvent(&event);
		if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
			return true;
		if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
			_drama->request_quit();
	}
	if (_drama && _drama->should_quit())
		return true;
	if (!_config.screenshot.empty() && _drama && _drama->captured())
		return true;
	if (!_config.capture_frames.empty() && _drama && _drama->captures_done())
		return true;
	if (!_config.probe_dir.empty() && _drama && (_drama->probes_done() || _drama->is_done()))
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
	_drama->render();
	_ui.end_frame();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	_drama->capture();
}

void Scene::update(float dt) { _drama->update(dt); }
