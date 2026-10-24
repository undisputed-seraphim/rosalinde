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
	_playback = std::make_unique<ScenePlayback>(_loader, _config);
	viewer::trace::stamp("Scene ctor: ScenePlayback ready");
}

bool Scene::handle_inputs() {
	SDL_Event event{};
	while (SDL_PollEvent(&event)) {
		ImGui_ImplSDL3_ProcessEvent(&event);
		if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
			return true;
		if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
			_playback->request_quit();
	}
	if (_playback && _playback->should_quit())
		return true;
	if (!_config.screenshot.empty() && _playback && _playback->captured())
		return true;
	if (!_config.capture_frames.empty() && _playback && _playback->captures_done())
		return true;
	if (!_config.probe_dir.empty() && _playback && (_playback->probes_done() || _playback->is_done()))
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
	_playback->render();
	_ui.end_frame();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	_playback->capture();
}

void Scene::update(float dt) { _playback->update(dt); }
