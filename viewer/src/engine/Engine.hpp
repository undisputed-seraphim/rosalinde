#pragma once

#include <memory>

#include "engine/Window.hpp"

namespace uvw {

class BaseGame {
public:
	virtual bool handle_inputs() = 0;
	virtual void render() = 0;
	virtual void update(unsigned dt) = 0;
};

class Engine final {
public:
	Engine(std::unique_ptr<BaseGame>&& game, const char* title, unsigned width = 1920, unsigned height = 1080);

	void run();

private:
	std::unique_ptr<BaseGame> _game;
	Window _window;
};

} // namespace uvw