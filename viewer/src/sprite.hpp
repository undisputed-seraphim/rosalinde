#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

#include <eltolinde.hpp>

#include "camera.hpp"
#include "glxx/buffers.hpp"

class Sprite {
private:
#pragma pack(push, 1)
	struct vertex {
		int16_t texid;
		glm::vec2 uv;
		glm::vec3 xyz;
		uint32_t color;
	};
#pragma pack(pop)
	gl::ArrayBuffer<vertex> _vertices;
	gl::uiElementBuffer _indices;

	MBS _mbs;
	std::vector<FTX::Entry> _textures;

	uint32_t _glTexHandle;
	uint32_t _flags;

	uint32_t _trackidx;

	std::vector<uint32_t> _frames;
	std::vector<uint32_t> _track;

public:
	Sprite(MBS, std::vector<FTX::Entry>, uint32_t flags, uint32_t trackid);
	Sprite(const Sprite&) = delete;
	Sprite(Sprite&&) noexcept = default;

	void play(uint32_t trackid);
	void render(Camera&, const glm::mat4& projection);
	void update(uint64_t);
};
