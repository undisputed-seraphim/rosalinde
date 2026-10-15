#pragma once

#include <string>
#include <vector>

namespace asb {

// A live scene object the engine would have created (a sprite or character),
// as recorded from the hosted module's create/mutate calls.
struct RuntimeEntity {
	// Which coordinate space the create call targeted. World entities are placed
	// by camera/pos in world units; UI entities live in the fixed 1920-wide
	// design space.
	enum class Space { World, Ui };

	std::string name;
	std::string kind; // originating create call
	std::vector<std::string> aliases;
	std::string mbs;
	std::string ftx;
	std::string track;
	int track_index = -1;
	float pos[3] = {0.0f, 0.0f, 0.0f};
	float scale = 1.0f;
	int layer = -1;
	bool visible = true;
	int order = 0;
	Space space = Space::World;
};

struct RuntimeCamera {
	float pos[3] = {0.0f, 0.0f, 0.0f};
	bool has_pos = false;
	float zoom = 0.0f;
	bool has_zoom = false;
	float accel = 0.0f;
	float decel = 0.0f;
};

// The whole scene at one frame. Only emitted when something changed.
struct Keyframe {
	long frame = 0;
	std::vector<RuntimeEntity> entities;
	RuntimeCamera camera;
};

struct Timeline {
	std::string source;
	std::string function;
	std::vector<Keyframe> keyframes;
};

} // namespace asb
