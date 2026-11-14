#include "engine/Engine.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

namespace uvw {

Engine::Engine(const char* title, unsigned width, unsigned height, bool vsync)
	: _window(title, width, height, vsync) {}

void Engine::run(std::function<std::unique_ptr<BaseGame>()> factory) {
	auto game = factory();
	const uint64_t freq = SDL_GetPerformanceFrequency();
	uint64_t last = SDL_GetPerformanceCounter();
	for (bool stop = false; !stop;) {
		const uint64_t now = SDL_GetPerformanceCounter();
		const double elapsed = static_cast<double>(now - last) / static_cast<double>(freq);
		last = now;
		const float dt = static_cast<float>(std::min(elapsed, 0.25));

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