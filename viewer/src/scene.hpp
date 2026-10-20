#pragma once

#include "asset_loader.hpp"
#include "debug_ui.hpp"
#include "drama_playback.hpp"
#include "engine/Engine.hpp"
#include "viewer_config.hpp"

#include <memory>

class Scene final : public uvw::BaseGame {
public:
	Scene(ViewerConfig config);

	bool handle_inputs() override;
	void render() override;
	void update(float dt) override;

private:
	ViewerConfig _config;
	AssetLoader _loader;
	DebugUI _ui;

	std::unique_ptr<DramaPlayback> _drama;
};
