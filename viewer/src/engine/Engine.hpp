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
    Engine(std::unique_ptr<BaseGame>&& game, const char* title, unsigned width = 1920, unsigned height = 1080) : _game(std::move(game)), _window(title, width, height) {}

    void run() {
        unsigned dt = 0;
        for (bool stop = false; !stop;) {
            stop = _game->handle_inputs();
            _game->update(dt);
            _window.swapbuffer();
        }
    }

private:
    std::unique_ptr<BaseGame> _game;
    Window _window;
};

}