#pragma once

#include <scarlett/asb.hpp>
#include <scarlett/scene.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <functional>
#include <memory>

class asIScriptGeneric;

namespace asb {

// The recording shim: models the scene-relevant native calls (create / mutate /
// camera) so a hosted module populates a runtime scene instead of hitting a
// no-op stub. Also owns the `Wait` scheduler hook.
class Registry {
public:
	// Display-name -> asset table used to resolve chara creates. Not owned.
	void set_char_table(const CharTable* chars) { _chars = chars; }

	// Engine CharaSet appearance table, used for world characters whose create
	// call carries no asset. Not owned.
	void set_chara_set_table(const CharaSetTable* sets) { _chara_sets = sets; }

	// Optional: animation length (in frames) for an asset/track, used to model
	// `WaitSpriteMoneAnimationEnd`. When unset (or returning 0) such waits stay
	// immediate.
	void set_track_frames(std::function<long(const std::string&, int)> fn) { _track_frames = std::move(fn); }

	// Optional: resolve an MBS track name to its index for an asset. When unset
	// (or returning < 0) a name-based animation call records the name but leaves
	// `track_index` at -1.
	void set_resolve_track(std::function<int(const std::string&, const std::string&)> fn) {
		_resolve_track = std::move(fn);
	}

	void reset(std::string source, std::string function);

	// Model a native call. Returns true if recognised (and thus not handled by
	// the fallback stub). `gen` is the AngelScript generic call context.
	bool dispatch(asIScriptGeneric* gen);

	// Snapshot the current state into a keyframe if it changed since the last.
	void flush_keyframe();

	long frame = 0;
	// Frames to sleep requested by the last `Wait`; -1 when unset.
	long pending_wait = -1;

	Timeline& timeline() { return _timeline; }

private:
	bool construct_value(asIScriptGeneric* gen);
	bool vec_binary(asIScriptGeneric* gen, std::string_view op);
	int ensure(const std::string& name);
	void alias(int idx, const std::string& name);
	void create_sprite(
		const std::string& kind,
		const std::string& id,
		std::string mbs,
		std::string ftx,
		const float* pos3,
		int layer,
		float scale);
	void create_chara(const std::string& kind, const std::vector<std::string>& names);

	// Screen-fade overlay colour (linear RGBA) at `frame`, and start a fade
	// towards `to` lasting `time` frames from the current frame.
	void fade_at(long frame, float* out) const;
	void push_fade(const float* to, long time);
	long fade_end() const;

	const CharTable* _chars = nullptr;
	const CharaSetTable* _chara_sets = nullptr;
	// Script-accumulated save/party state: recruited chara def names and the
	// integer slot-flag store. Read back by the state getters.
	std::unordered_set<std::string> _party;
	std::unordered_map<int, int> _slots;
	std::unordered_set<std::string> _class_changed;
	std::unordered_set<std::string> _disabled;
	std::unordered_map<int, int> _quests;			// quest id -> bits (1 open, 2 completed)
	std::unordered_map<std::string, int> _unit_dir; // unit name -> direction/degree
	std::unordered_set<int> _units;					// recruited unit ids (AddUnitSaveData)
	// Movement tweens (Move/MoveAbs/DrMove*): interpolate an entity's position
	// from `from` to `to` over [f0, f1]; advanced in flush_keyframe.
	struct Move {
		float from[3];
		float to[3];
		long f0 = 0, f1 = 0;
	};
	std::unordered_map<std::string, Move> _moves;
	std::unordered_map<std::string, int> _db_params; // DbSet/GetDebugParam + LocalParam
	std::unordered_set<std::string> _help;			 // AddToHelpList

	// Dialogue / text (Phase 4a). UcMsgBalloonParam and UcSpriteTextParam are
	// engine reference objects: scripts mutate them through native setters and
	// then pass them to Msg/CreateSpriteText, so their state is kept per object.
	struct BalloonParam {
		int disp_type = -1;
		int disp_name = -1;
		int face_dir_x = 0;
		unsigned flags = 0;
	};
	struct SpriteTextParam {
		int align = 0;
		int font_type = 0;
		int outline = 0;
		int color[3] = {255, 255, 255};
		float scale = 1.0f;
		float base_line = 0.0f;
		bool force_instant = false;
	};
	std::unordered_map<const void*, BalloonParam> _balloons;
	std::unordered_map<const void*, SpriteTextParam> _sprite_texts;
	// Factories for the engine's opaque reference types return handles, so the
	// allocated shim objects are owned here (they must outlive the script).
	std::vector<std::unique_ptr<BalloonParam>> _balloon_owner;
	std::vector<std::unique_ptr<SpriteTextParam>> _sprite_text_owner;
	// Opaque storage for engine reference-type factories we don't model yet
	// (UcStObject, UcStChaseParam, ...): scripts get a non-null handle.
	std::vector<std::unique_ptr<char[]>> _opaque_owner;
	int _select_result = 0; // last GetResultSelectMsg / GetYNDialogResult
	int _sprite_text_handle = 0;
	std::function<long(const std::string&, int)> _track_frames;
	std::function<int(const std::string&, const std::string&)> _resolve_track;
	Timeline _timeline;
	std::vector<RuntimeEntity> _entities;
	std::unordered_map<std::string, int> _by_name;
	RuntimeCamera _camera;
	bool _dirty = false;
};

} // namespace asb
