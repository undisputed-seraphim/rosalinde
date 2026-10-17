#pragma once

#include "asset_loader.hpp"
#include "camera.hpp"
#include "effect_manager.hpp"
#include "light_renderer.hpp"
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
	void apply_state(long frame);
	void tick(float dt);
	glm::vec2 entity_pos(const std::string& name) const;

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
	ScreenFadeOverlay _overlay;
	EffectManager _effects;
	LightRenderer _lights_renderer;
	glm::vec2 _shake_offset{0.0f, 0.0f};
	std::vector<asb::RuntimeShake> _shakes;
	size_t _shake_i = 0, _fog_i = 0, _bok_i = 0, _var_i = 0;
	glm::vec4 _fog{0.0f, 0.0f, 0.0f, 0.0f};
	asb::RuntimeBokashi _bok{};
	bool _bok_active = false;
	unsigned _variation = 0;
	std::unordered_map<std::string, asb::RuntimePointLight> _lights;
	size_t _light_i = 0;

	long _cap_from = -1;
	long _cap_to = -1;
	long _cap_every = 1;
	std::string _cap_dir;
	bool _cap_started = false;

	int _focus = -1; // last entity to become active (camera "follow")
	std::vector<std::pair<long, glm::vec2>> _cam_keys; // recorded DrSetCameraPosV keys

	struct CamKey {
		long frame;
		glm::vec2 pos;
		float zoom;
	};
	std::vector<CamKey> _cam_rec; // recorded camera keys (pos + zoom) for "record"

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
