#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "shader.hpp"
#include "texture.hpp"

class State {
    uint32_t _tgt_fb;

public:
    State();
    State(const State&) = delete;
    State(State&&) noexcept = default;
    ~State() noexcept;

    struct MbsFtx {
        std::string mbs, ftx;
        std::vector<uint32_t> variations;
    };
    static const std::unordered_map<std::string, MbsFtx> Chara;

    void render();
};

namespace gl {

void depth(int d = 0);

}