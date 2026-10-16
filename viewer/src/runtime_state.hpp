#pragma once

#include "asset_loader.hpp"
#include "camera.hpp"
#include "screen_fade.hpp"
#include "sprite_layer.hpp"

#include <scarlett/scene.hpp>

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

struct RuntimeSceneConfig {
	std::string entry;			   // e.g. "Script/Dr_OW_0_Opening.asb"
	std::string func;			   // scene function (default: pick_scene_function)
	int limit = 256;			   // max sprite layers to load
	std::string camera = "design"; // design (origin-centred) | fit | first | last | <index>
	std::string chara_table;
	std::string screenshot;
	uint32_t frames = 1; // rendered frames before capture
	long frame = -1;	 // scene frame to advance to (-1 = real time)
	float fps = 60.0f;
};

// Plays a scene reconstructed by the hosted AngelScript VM: run the module to a
// Timeline, then apply keyframes against a frame clock and tick MBS animations.
class RuntimeState {
public:
	RuntimeState(const AssetLoader& loader, RuntimeSceneConfig cfg);

	bool handle_inputs();
	void update(float dt);
	void render();

	bool is_done() const { return _done; }
	bool should_quit() const { return _quit; }
	bool captured() const { return _captured; }

private:
	void apply_keyframe();
	void tick(float dt);

	const AssetLoader& _loader;
	RuntimeSceneConfig _cfg;
	Camera _camera;
	glm::mat4 _projection;

	asb::Timeline _timeline;
	std::vector<std::unique_ptr<SpriteLayer>> _layers; // aligned by entity order
	std::vector<char> _active;
	std::vector<int> _track;
	std::vector<int> _parent; // entity index this layer is anchored to, or -1
	ScreenFadeOverlay _fade;

	long _cap_from = -1;
	long _cap_to = -1;
	long _cap_every = 1;
	std::string _cap_dir;
	bool _cap_started = false;

	size_t _kf = 0;
	long _frame = 0;
	double _accum = 0.0;
	bool _playing = true;
	bool _stepped = false;
	uint32_t _render_count = 0;
	bool _captured = false;
	bool _done = false;
	bool _quit = false;
};
