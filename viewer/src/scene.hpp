#pragma once

#include "camera.hpp"
#include "sprite.hpp"
#include "sprite_renderer.hpp"
#include "engine/Engine.hpp"

#include <criware/cpk.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

struct SpriteLayer {
	std::string name;
	SpriteData data;
	SpriteInstance instance;
	SpriteRenderer renderer;
};

class Scene final : public uvw::BaseGame {
public:
	Scene(std::filesystem::path cpkpath, const std::string& classname,
		const std::string& charaname, const std::string& classname2,
		const std::string& charaname2, uint32_t trackid, bool debug,
		std::string screenshot_path = {});

	~Scene() noexcept;

	bool handle_inputs() override;
	void render() override;
	void update(float dt) override;

private:
	CPKTable _cpkt;
	Camera _camera;
	glm::mat4 _projection;

	std::vector<std::unique_ptr<SpriteLayer>> _layers;
	size_t _active_layer = 0;

	std::string _screenshot_path;
	bool _done = false;
	bool _captured = false;

	std::unique_ptr<SpriteLayer> load_layer(const struct Job& job, uint32_t variant_flags, uint32_t trackid) const;
};
