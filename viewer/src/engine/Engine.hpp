#pragma once

#include <array>
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

	void set_clear_color(const std::array<float, 4>& color) noexcept { _window.set_clear_color(color); }
	void run(std::function<std::unique_ptr<BaseGame>()> factory);

private:
	Window _window;
};

} // namespace uvw
