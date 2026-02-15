#include "engine/Engine.hpp"

namespace uvw {

Engine::Engine(std::unique_ptr<BaseGame>&& game, const char* title, unsigned width, unsigned height)
	: _game(std::move(game))
	, _window(title, width, height) {}

void Engine::run() {
	unsigned dt = 0;
	for (bool stop = false; !stop;) {
		stop = _game->handle_inputs();
		_game->update(dt);
		_window.swapbuffer();
	}
}

} // namespace uvw