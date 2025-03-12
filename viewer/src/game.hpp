#pragma once

#include "engine/Engine.hpp"

class Game final : public uvw::BaseGame {
public:
    bool handle_inputs() override;

    void render() override;

    void update(unsigned) override;
};