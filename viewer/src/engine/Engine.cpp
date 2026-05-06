#include "engine/Engine.hpp"

#include <SDL3/SDL.h>

namespace uvw {

Engine::Engine(std::unique_ptr<BaseGame>&& game, const char* title, unsigned width, unsigned height)
	: _game(std::move(game))
	, _window(title, width, height) {}

void Engine::run() {
	uint64_t last = SDL_GetTicks();
	for (bool stop = false; !stop;) {
		uint64_t now = SDL_GetTicks();
		float dt = (now - last) / 1000.0f;
		last = now;
		if (dt > 0.25f) dt = 0.25f;

		stop = _game->handle_inputs();
		_game->update(dt);

		if (!(_window.flags() & SDL_WINDOW_MINIMIZED)) {
			_window.clear();
			_game->render();
			_window.swapbuffer();
		} else {
			SDL_Delay(10);
		}
	}
}

} // namespace uvw