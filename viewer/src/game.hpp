#pragma once

#include <vector>

#include "camera.hpp"
#include "sprite.hpp"
#include "engine/Engine.hpp"

class Game final : public uvw::BaseGame {
private:
	Camera _camera;

	std::vector<Sprite> _sprites;

public:
	bool handle_inputs() override;

	void render() override;

	void update(unsigned) override;
};