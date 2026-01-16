#pragma once

#include <criware/cpk.hpp>
#include <eltolinde.hpp>
#include <filesystem>
#include <string>
#include <vector>

#include "camera.hpp"
#include "shader.hpp"
#include "sprite.hpp"

class State {
	CPKTable _cpkt;
	uint32_t _tgt_fb;
	std::vector<char> _buffer;

	Camera _camera;

	std::vector<Sprite> _sprites;

public:
	State(std::filesystem::path);
	State(const State&) = delete;
	State(State&&) noexcept = default;
	~State() noexcept;

	void loadSprite(const std::string& classname, const std::string& charaname, uint32_t trackid = 0);

	void handleEvent(const SDL_Event&);

	void render(Camera& cam, const glm::mat4& projection);
};
