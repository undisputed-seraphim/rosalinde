#include <SDL3/SDL.h>

#include "game.hpp"

bool Game::handle_inputs() {
	bool done = false;
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			done = true;
		}
		if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
			done = true;
		}
		if (event.type == SDL_EVENT_MOUSE_WHEEL) {
			//cam.zoom(event.wheel.y);
		}
		if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			//cam.enter();
		}
		if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
			//cam.exit();
		}
		//cam.move(event.motion.xrel, event.motion.yrel);
	}
	return done;
}