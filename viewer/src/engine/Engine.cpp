#include "engine/Engine.hpp"

#include <SDL3/SDL.h>

namespace uvw {

Engine::Engine(const char* title, unsigned width, unsigned height)
	: _window(title, width, height) {}

void Engine::run(std::function<std::unique_ptr<BaseGame>()> factory) {
	auto game = factory();
	uint64_t last = SDL_GetTicks();
	for (bool stop = false; !stop;) {
		uint64_t now = SDL_GetTicks();
		float dt = (now - last) / 1000.0f;
		last = now;
		if (dt > 0.25f) dt = 0.25f;

		stop = game->handle_inputs();
		game->update(dt);

		if (!(_window.flags() & SDL_WINDOW_MINIMIZED)) {
			_window.clear();
			game->render();
			_window.swapbuffer();
		} else {
			SDL_Delay(10);
		}
	}
}

} // namespace uvw