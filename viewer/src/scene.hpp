#pragma once

#include "camera.hpp"
#include "sprite.hpp"
#include "sprite_renderer.hpp"
#include "engine/Engine.hpp"

#include <criware/cpk.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <string>

class Scene final : public uvw::BaseGame {
public:
	Scene(std::filesystem::path cpkpath, const std::string& classname,
		const std::string& charaname, uint32_t trackid, bool debug);

	~Scene() noexcept;

	bool handle_inputs() override;
	void render() override;
	void update(float dt) override;

private:
	CPKTable _cpkt;
	Camera _camera;
	glm::mat4 _projection;

	SpriteData _data;
	SpriteInstance _instance;
	SpriteRenderer _renderer;
};
