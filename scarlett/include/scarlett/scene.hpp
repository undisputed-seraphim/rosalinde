#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
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
	float dir_x = 1.0f; // facing: -1 mirrored, +1 default
	int alpha = 255;	// 0-255, from DrUiSetColorA (0 = not yet faded in)
	int layer = -1;
	bool visible = true;
	bool reverse = false; // SetSpriteMoneReverse: play the selected track backward
	unsigned layer_mask = 0xFFFFFFFFu; // DrSetLayerMask: visible layer bits
	bool breath_off = false;		   // DrSetBreathOff
	bool lipsync = false;			   // DrSetLipSync
	bool shadow_off = false;		   // DrSetShadowOff
	int sort_id_offset = 0;			   // DrSetSortIDOffset
	int dlprio = 0;					   // DrSetDLPrio
	int window_id_offset = 0;		   // DrSetWindowIDOffset
	bool prerender_force = false;	   // DrSetPrerenderOnForce
	int color[4] = {-1, -1, -1, -1};   // DrSetCharaColor (common/hair/main/sub)
	int state = 0;					   // FlSetState / FlAddState
	int state_icon = 0;				   // FlSetStateIcon / FlClearStateIcon
	float height = 0.0f;			   // FlSetHeight
	int emote = 0;					   // FlEmote
	bool search_flag = false;		   // FlSetSearchFlag
	bool paused = false;			   // FlPauseSymbol / FlUnpauseSymbol (freezes animation)
	bool anim_paused = false;		   // SetSpriteMoneAnimationPaused
	bool ai_paused = false;			   // FlSetMoveAiPermissionInDemo (no AI yet)
	bool auto_dir_off = false;		   // FlSetAutoDirOff
	bool reverse_x = false;			   // FlSetReverseX (mirrors X)
	int order = 0;
	Space space = Space::World;
	std::string parent; // SetSpriteToAnchor target (empty = none); pos is relative
};

struct RuntimeCamera {
	float pos[3] = {0.0f, 0.0f, 0.0f};
	bool has_pos = false;
	float zoom = 0.0f;
	bool has_zoom = false;
	float accel = 0.0f;
	float decel = 0.0f;
	bool tebure = false;	   // DrTebureCamera (hand-held offset; rendering deferred)
	bool wave = false;		   // DrWaveCamera
	float demo_y = 0.0f;	   // SetDemoCameraPosY / ResetDemoCameraPosY
	float demo_xz[2] = {0.0f, 0.0f}; // ResetDemoCameraPosXZ
	float degree_offset = 0.0f;		 // SetCameraDegreeOffset
};

// A whole-screen colour fade (CrossFadeAsync/ColorFadeAsync). The overlay
// colour is lerped from `from` to `to` (linear RGBA, 0..1) over [start, end].
struct ScreenFade {
	long start = 0;
	long end = 0;
	float from[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float to[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

// A dialogue / balloon message emitted by the Msg / Dialog / Select family.
// `speaker` is the object name (Msg arg 0); `text` is the literal message text
// the script itself passes (Msg arg 1). `ids` is the _nMsgIDArray argument.
struct RuntimeText {
	enum class Kind { Msg, Screen, Dialog, Select, YesNo, Message };

	long frame = 0;
	long end = 0; // frame the message is dismissed (0 = immediate)
	Kind kind = Kind::Msg;
	std::string speaker;
	std::string text;
	std::vector<int> ids;
	float pos[2] = {0.0f, 0.0f};
	int disp_type = -1; // UcMsgBalloonParam
	int disp_name = -1;
	int face_dir_x = 0;
	unsigned flags = 0;
	int result = -1; // chosen index for Select/YesNo
};

// DrCreateStage(stageType, weatherType, coaPreset, flagColorIndex).
struct StageConfig {
	long frame = 0;
	int stage_type = 0;
	int weather_type = 0;
	int coa_preset = 0;
	int flag_color = 0;
};

// DrSetShake / DrStopShake. The structured overload carries the full curve
// parameters; the int overload selects an engine preset.
struct RuntimeShake {
	long start = 0;
	long end = 0;
	int index = 0;
	int type = -1; // int-overload preset
	float shake[2] = {0.0f, 0.0f};
	float angle = 0.0f;
	float att = 0.0f;
	float pow = 0.0f;
};

// BokashiFadeAsync: a gradation/freeze fade.
struct RuntimeBokashi {
	long start = 0;
	long end = 0;
	bool bokashi = true;
	bool freeze = true;
	float max_rate = 1.0f;
};

// DrSetVariationType: a global variation bitmask change.
struct VariationEvent {
	long frame = 0;
	unsigned mask = 0;
};

// Record-only effect spawn/mutation (DrCreateEffect/DrSetEffect*/DrMoveEffect/
// DrScaleEffect/DrRotateEffect). Rendering is Phase 8.
struct RuntimeEffect {
	long frame = 0;
	std::string kind;
	std::string name;
	std::string mbs;
	std::string ftx;
	std::string attached;
	int anime = 0;
	float pos[3] = {0.0f, 0.0f, 0.0f};
	int i0 = 0, i1 = 0, i2 = 0;
	float f0 = 0.0f;
	bool flag = false;
};

struct RuntimePointLight {
	long frame = 0;
	bool remove = false;
	std::string name;
	float pos[3] = {0.0f, 0.0f, 0.0f};
	unsigned char light[4] = {0, 0, 0, 0};
	unsigned char shadow[4] = {0, 0, 0, 0};
	unsigned char godray[4] = {0, 0, 0, 0};
	float power = 0.0f;
	float bloom = 0.0f;
	float f2 = 0.0f, f3 = 0.0f, f4 = 0.0f, f5 = 0.0f;
	std::string attached;
	int index = 0;
};

// Record-only audio event (BGM/SE/voice/ambience/volume). No playback.
struct RuntimeAudio {
	long frame = 0;
	std::string kind;
	int cue = 0;
	std::string target;
	int fade = 0;
	int wait = 0;
	int index = -1;
	float volume = 0.0f;
};

// Script callback registration. The target is a module function name; actual
// dispatch is deferred until the corresponding game event exists.
struct RuntimeCallback {
	long frame = 0;
	std::string event;
	std::string key;
	std::string slot;
	std::string function;
};

// Item/treasure record. The inventory count side is Phase 6; the dialog popup
// is Phase 5C; this is the event itself.
struct RuntimeItem {
	long frame = 0;
	std::string kind;
	std::string object;
	std::string item;
	int id = 0;
	int count = 0;
	float pos[2] = {0.0f, 0.0f};
	bool dialog = false;
	std::string callback;
	int add[8] = {0};
	int add_count = 0;
};

struct RuntimeFog {
	long frame = 0;
	std::string name;
	unsigned char color[4] = {0, 0, 0, 0};
};

struct RuntimeGradient {
	long frame = 0;
	int index = 0;
	float rate = 0.0f;
	int time = 0;
};

// 5C UI / archive / HUD event (record-only; rendering deferred).
struct RuntimeUi {
	long frame = 0;
	std::string kind;
	std::string text;
	int id = 0;
	int i0 = 0, i1 = 0;
	float pos[2] = {0.0f, 0.0f};
	float rate = 0.0f;
	bool flag = false;
	std::vector<int> ints; // archive pages, etc.
};

// Phase 6 field/overworld event (record-only where visual).
struct RuntimeField {
	long frame = 0;
	std::string kind;
	std::string name;
	std::string text;
	std::string text2;
	int i0 = 0, i1 = 0, i2 = 0;
	float f0 = 0.0f;
	float pos[2] = {0.0f, 0.0f};
	bool flag = false;
	std::vector<int> ints;
};

// The whole scene at one frame. Only emitted when something changed.
struct Keyframe {
	long frame = 0;
	std::vector<RuntimeEntity> entities;
	RuntimeCamera camera;
};

// Live scene state owned by the runtime. The native shim mutates this in place;
// renderers and tools read it. `Timeline` adds the offline keyframe recording.
struct SceneState {
	std::string source;
	std::string function;
	long frame = 0;
	uint64_t revision = 0;
	std::vector<RuntimeEntity> entities;
	RuntimeCamera camera;
	std::vector<ScreenFade> fades;
	std::vector<StageConfig> stages;
	std::vector<RuntimeText> texts;
	std::vector<RuntimeShake> shakes;
	std::vector<RuntimeBokashi> bokashi;
	std::vector<VariationEvent> variations;
	std::vector<RuntimeEffect> effects;
	std::vector<RuntimePointLight> point_lights;
	std::vector<RuntimeAudio> audio;
	std::vector<RuntimeItem> items;
	std::vector<RuntimeCallback> callbacks;
	std::vector<RuntimeFog> fog;
	std::vector<RuntimeGradient> gradients;
	std::vector<RuntimeUi> ui;
	std::vector<RuntimeField> field;

	const RuntimeEntity* find_entity(std::string_view name) const {
		for (const auto& e : entities) {
			if (e.name == name)
				return &e;
			for (const auto& a : e.aliases)
				if (a == name)
					return &e;
		}
		return nullptr;
	}
};

struct Timeline : SceneState {
	std::vector<Keyframe> keyframes;
};

// Overlay colour (linear RGBA) produced by the fades list at `frame`.
inline void fade_at(const std::vector<ScreenFade>& fades, long frame, float* out) {
	out[0] = out[1] = out[2] = out[3] = 0.0f;
	for (const auto& s : fades) {
		if (frame >= s.end) {
			for (int k = 0; k < 4; ++k)
				out[k] = s.to[k];
		} else if (frame >= s.start) {
			const float t = static_cast<float>(frame - s.start) / static_cast<float>(std::max(1L, s.end - s.start));
			for (int k = 0; k < 4; ++k)
				out[k] = s.from[k] + (s.to[k] - s.from[k]) * t;
		}
	}
}

} // namespace asb
