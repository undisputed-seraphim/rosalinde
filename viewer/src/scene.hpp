#pragma once

#include "camera.hpp"
#include "sprite.hpp"
#include "sprite_renderer.hpp"
#include "engine/Engine.hpp"

#include <criware/cpk.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct SpriteLayer {
	std::string name;
	std::string class_name;
	std::string variant_name;
	glm::vec2 position{0, 0};
	SpriteData data;
	SpriteInstance instance;
	SpriteRenderer renderer;
	uint32_t default_flags = 0;
	std::map<uint32_t, glm::vec4> layer_tints;
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
	std::vector<std::string> _class_names;
	size_t _active_layer = 0;

	std::string _screenshot_path;
	bool _done = false;
	bool _captured = false;
	int _variant_side = 0;

	std::unique_ptr<SpriteLayer> load_layer(const struct Job& job, uint32_t trackid,
		const std::string& class_name, const std::string& variant_name) const;
};
