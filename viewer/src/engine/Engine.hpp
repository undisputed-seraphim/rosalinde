#pragma once

#include <functional>
#include <memory>

#include "engine/Window.hpp"

namespace uvw {

class BaseGame {
public:
	virtual ~BaseGame() = default;
	virtual bool handle_inputs() = 0;
	virtual void render() = 0;
	virtual void update(float dt) = 0;
};

class Engine final {
public:
	Engine(const char* title, unsigned width = 1920, unsigned height = 1080);

	void run(std::function<std::unique_ptr<BaseGame>()> factory);

private:
	Window _window;
};

} // namespace uvw
