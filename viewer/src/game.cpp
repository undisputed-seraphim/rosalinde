#include <SDL3/SDL.h>

#include "game.hpp"

bool Game::handle_inputs() {
	bool done = false;
	SDL_Event event{};
	while (SDL_PollEvent(&event)) {
		// ImGui_ImplSDL3_ProcessEvent(&event);
		switch (event.type) {
		case SDL_EVENT_QUIT:
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
			done = true;
			break;
		}
		case SDL_EVENT_KEY_DOWN: {
			switch (event.key.key) {
			case SDLK_DOWN: {
				--_sprites[0];
				break;
			}
			case SDLK_UP: {
				++_sprites[0];
				break;
			}
			}
			break;
		}
		}
		_camera.handleInput(event);
	}
	return done;
}

void Game::render() {}

void Game::update(unsigned) {}