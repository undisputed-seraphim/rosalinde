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

	// Optional facility display-name resolver (`GetStationFacilityName`).
	void set_facility_name(std::function<std::string(const std::string&)> fn) { _facility_name = std::move(fn); }

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
		float arc = 0.0f; // DrJumpAsync: height added as a parabola over the tween
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

	// Drama playback state (Phase 5A).
	unsigned _variation = 0;	  // DrSetVariationType (global bitmask)
	bool _skip_mode = false;	  // IsSkipMode / CancelSkipMode
	long _camera_move_end = 0;	  // DrCameraMoveWait
	int _shake_seq = 0;			  // DrSetShake handle counter
	float _follow_offset[2] = {0.0f, 0.0f}; // FollowOffsetV
	bool _balcony_bru = false;	  // DrEnableBalconyFlagBRU / DrGetBalconyFlagBRU
	bool _balcony_scr = false;	  // DrEnableBalconyFlagSCR / DrGetBalconyFlagSCR
	// 5C UI / archive / HUD state.
	bool _partner_select_open = false;
	bool _disable_backlog = false;
	bool _disable_option = false;
	bool _title_library = false;
	bool _class_pedia_open = false;
	bool _all_class_pedia_bg = false;
	bool _add_class_pedia_flag = false;
	bool _staff_roll_scroll = false;
	float _staff_roll_speed = 0.0f;

	// Phase 6 field/overworld + game-state.
	int _gold = 0;
	int _fame = 0;
	std::unordered_map<int, int> _stage_flags;
	std::unordered_set<int> _stage_bits;
	std::unordered_map<int, int> _global_flags;
	std::unordered_map<std::string, int> _items;
	std::unordered_set<std::string> _facilities;
	std::unordered_map<std::string, int> _friendship;
	std::unordered_set<int> _treasures;
	std::unordered_set<int> _quest_pinned;
	std::unordered_set<int> _quest_preopened;
	std::unordered_map<std::string, int> _town_lv;
	int _clock_hour = 0, _clock_minute = 0, _clock_second = 0;
	bool _clock_paused = false;
	bool _demo_mode = false;
	bool _world_map_disabled = false;
	bool _battle_mode = false;
	std::string _battle_after, _battle_die_after;
	std::string _player_symbol;
	float _revive_pos[2] = {0.0f, 0.0f};
	int _item_change_shop_level = 0;
	int _item_change_num = 0;
	int _ex_stage_dialog_result = 0;
	int _stage_start_param = 0;

	// Phase 7 battle / AI. The engine owns the simulation (unit turn loop, AI
	// component list, gambit execution, engagement/damage, reinforcements); the
	// stores below only keep return values coherent. TODO(phase7): implement the
	// simulation.
	struct BattleUnit {
		std::string name;
		std::string def;
		float pos[2] = {0.0f, 0.0f};
		int army = 0;
		int state = 0;
		int hp = 100;
		int stamina = 0;
		int brave = 0;
		bool demo = false;
		bool vehicle = false;
	};
	struct BattleTimer {
		long start = 0;
		int rest = 0;
		bool running = false;
	};
	std::unordered_map<std::string, BattleUnit> _battle_units;
	std::unordered_map<std::string, BattleTimer> _battle_timers;
	std::unordered_map<std::string, std::string> _unit_station;
	std::unordered_map<std::string, int> _facility_army;
	std::unordered_set<std::string> _selectable_objects;
	std::unordered_set<std::string> _targetable_objects;
	std::unordered_set<int> _selectable_commands;
	int _selected_command = -1;
	std::string _battle_scene;
	std::string _battle_bg;
	bool _battle_scene_on = false;
	int _time_limit_day = -1, _time_limit_hour = -1, _time_limit_minute = -1;
	int _stage_unit_base_lv = 0;
	int _battle_unit_seq = 0;
	int _battle_brave = 0;
	std::string _battle_found_last;
	std::vector<std::string> _partner_defs;
	std::vector<std::string> _battle_unit_order;
	bool dispatch_battle(std::string_view name, asIScriptGeneric* gen);

	std::function<long(const std::string&, int)> _track_frames;
	std::function<int(const std::string&, const std::string&)> _resolve_track;
	std::function<std::string(const std::string&)> _facility_name;
	Timeline _timeline;
	std::vector<RuntimeEntity> _entities;
	std::unordered_map<std::string, int> _by_name;
	RuntimeCamera _camera;
	bool _dirty = false;
};

} // namespace asb
