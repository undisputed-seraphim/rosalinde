#include "engine/Engine.hpp"

namespace uvw {

Engine::Engine(const char* title, unsigned width, unsigned height)
	: _window(title, width, height) {}

void Engine::run(std::function<std::unique_ptr<BaseGame>()> factory) {
	auto game = factory();
	unsigned dt = 0;
	for (bool stop = false; !stop;) {
		stop = game->handle_inputs();
		game->update(dt);
		_window.clear();
		game->render();
		_window.swapbuffer();
	}
}

} // namespace uvw