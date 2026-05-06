#pragma once

#include <memory>

#include "engine/Window.hpp"

namespace uvw {

class BaseGame {
public:
	virtual ~BaseGame() = default;
	virtual bool handle_inputs() = 0;
	virtual void render() = 0;
	virtual void update(unsigned dt) = 0;
};

class Engine final {
public:
	Engine(std::unique_ptr<BaseGame>&& game, const char* title, unsigned width = 1920, unsigned height = 1080);

	void run();

private:
	Window _window;
	std::unique_ptr<BaseGame> _game;
};

} // namespace uvw