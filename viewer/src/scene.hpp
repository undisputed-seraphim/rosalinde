#pragma once

#include "camera.hpp"
#include "sprite.hpp"
#include "engine/Engine.hpp"

#include <criware/cpk.hpp>
#include <eltolinde.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include <vector>

class Scene final : public uvw::BaseGame {
public:
	Scene(std::filesystem::path cpkpath,
		const std::string& classname,
		const std::string& charaname,
		uint32_t trackid,
		bool debug);

	~Scene() noexcept;

	bool handle_inputs() override;
	void render() override;
	void update(unsigned dt) override;

private:
	void loadSprite(const std::string& classname, const std::string& charaname, uint32_t trackid);

	CPKTable _cpkt;
	Camera _camera;
	std::vector<Sprite> _sprites;
	glm::mat4 _projection;
	unsigned int _vao;
};
