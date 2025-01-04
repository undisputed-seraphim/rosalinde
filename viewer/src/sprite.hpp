#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

#include "glxx/buffers.hpp"

class Sprite {
private:
#pragma pack(push, 1)
    struct vertex {
        int16_t texid;
        glm::mat4x2 uv;
        glm::mat4x3 xyx;
        uint32_t color;
    };
#pragma pack(pop)
    std::vector<vertex> _vertices;
	gl::uiElementBuffer _indices;
public:

    void step() {
        //foreach track
            //foreach layer
    }

    // Function to set the animation
};
