#include "native_args.hpp"
#include "registry.hpp"

#include <angelscript.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace asb {

// Phase 7 battle / AI. The scripts only *configure* the engine's battle system:
// units, their AI component lists, gambit rules and reinforcement waves. The
// turn loop, component ticking, target search, engagement/damage and victory
// conditions are engine-native. This shim keeps the structural state scripts
// read back (unit names, state, hp/stamina/brave, timers, stations) and records
// the rest as no-ops. TODO(phase7): implement the battle simulation.
DispatchResult Registry::dispatch_battle(std::string_view name, NativeArgs& args) {
	asIScriptGeneric* gen = args.generic();
	auto S = [&](int i) { return args.str(i); };
	auto I = [&](int i) { return static_cast<int>(gen->GetArgDWord(i)); };
	auto F = [&](int i) { return gen->GetArgFloat(i); };
	auto ret_str = [&](std::string s) {
		if (auto* out = static_cast<std::string*>(gen->GetAddressOfReturnLocation()))
			new (out) std::string(std::move(s));
	};
	auto unit = [&](const std::string& n) -> BattleUnit* {
		auto it = _battle_units.find(n);
		return it == _battle_units.end() ? nullptr : &it->second;
	};
	auto make_unit =
		[&](const std::string& obj, const std::string& def, const float* p, int army, bool demo, bool vehicle) {
			std::string key = obj.empty() ? std::string("unit#") + std::to_string(++_battle_unit_seq) : obj;
			BattleUnit u;
			u.name = key;
			u.def = def.empty() ? key : def;
			if (p) {
				u.pos[0] = p[0];
				u.pos[1] = p[1];
			}
			u.army = army;
			u.demo = demo;
			u.vehicle = vehicle;
			_battle_units[key] = u;
			_battle_unit_order.push_back(key);
			return key;
		};
	auto army_count = [&](int army) {
		int n = 0;
		for (const auto& kv : _battle_units)
			if (!kv.second.vehicle && kv.second.army == army)
				++n;
		return n;
	};

	// ---- unit lifecycle -------------------------------------------------
	if (name == "StCreateUnit" || name == "StCreateTempUnit" || name == "StCreateEntryUnit") {
		// Instantiates a stage unit from the stage's unit-set table at a position
		// and returns the engine object name used by later calls.
		// TODO(phase7): real def/stats from the unit-set table.
		float p[2] = {0.0f, 0.0f};
		args.vec2(2, p);
		ret_str(make_unit(S(0), "set:" + std::to_string(I(1)), p, 0, false, false));
		return DispatchResult::Handled;
	}
	if (name == "StCreateDemoUnit") {
		// Demo units name a chara def (5 args) or a unit set (4 args).
		float p[2] = {0.0f, 0.0f};
		args.vec2(2, p);
		if (gen->GetArgCount() >= 5)
			make_unit(S(0), S(1), p, I(4), true, false);
		else
			make_unit(S(0), "set:" + std::to_string(I(1)), p, 0, true, false);
		return DispatchResult::Handled;
	}
	if (name == "StCreateVehicle") {
		float p[2] = {0.0f, 0.0f};
		args.vec2(2, p);
		make_unit(S(0), "vehicle:" + std::to_string(I(1)), p, I(3), false, true);
		return DispatchResult::Handled;
	}
	if (name == "StDeleteAllUnit") {
		_battle_units.clear();
		_battle_unit_order.clear();
		return DispatchResult::Handled;
	}
	if (name == "StDeleteAllDemoUnit" || name == "StDeleteAllVehicle") {
		const bool demo = name == "StDeleteAllDemoUnit";
		for (auto it = _battle_units.begin(); it != _battle_units.end();) {
			if (demo ? it->second.demo : it->second.vehicle)
				it = _battle_units.erase(it);
			else
				++it;
		}
		return DispatchResult::Handled;
	}
	if (name == "StLeave") {
		_battle_units.erase(S(0));
		return DispatchResult::Handled;
	}

	if (name == "StChangeArmyType") {
		if (auto* u = unit(S(0)))
			u->army = I(1);
		return DispatchResult::Handled;
	}
	if (name == "StSetState") {
		if (auto* u = unit(S(0)))
			u->state = I(1);
		return DispatchResult::Handled;
	}
	if (name == "StGetState") {
		auto* u = unit(S(0));
		gen->SetReturnDWord(u ? static_cast<asDWORD>(u->state) : 0u);
		return DispatchResult::Handled;
	}
	if (name == "StSetHp") {
		if (auto* u = unit(S(0)))
			u->hp = I(2);
		return DispatchResult::Handled;
	}
	if (name == "StHeal") {
		if (auto* u = unit(S(0))) {
			u->hp = std::max(u->hp, I(1));
			if (I(2) && u->hp <= 0)
				u->hp = 1;
		}
		return DispatchResult::Handled;
	}
	if (name == "StHealAllPlayerUnit") {
		for (auto& kv : _battle_units)
			if (kv.second.army == 0)
				kv.second.hp = 100;
		return DispatchResult::Handled;
	}
	if (name == "StGetStamina") {
		auto* u = unit(S(0));
		gen->SetReturnDWord(u ? static_cast<asDWORD>(u->stamina) : 0u);
		return DispatchResult::Handled;
	}
	if (name == "StAddStamina") {
		if (auto* u = unit(S(0)))
			u->stamina += I(1);
		return DispatchResult::Handled;
	}
	if (name == "StSetBrave") {
		_battle_brave = I(0);
		return DispatchResult::Handled;
	}
	if (name == "StAddBrave") {
		_battle_brave += I(0);
		return DispatchResult::Handled;
	}
	if (name == "StIsLowerHpRate") {
		auto* u = unit(S(0));
		gen->SetReturnByte((u && u->hp < I(1)) ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "StIsLowerHpRateByChara") {
		bool hit = false;
		for (const auto& kv : _battle_units)
			if (kv.second.def == S(0) && kv.second.hp < I(1))
				hit = true;
		gen->SetReturnByte(hit ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "StSetStageUnitBaseLv") {
		_stage_unit_base_lv = I(0);
		return DispatchResult::Handled;
	}
	if (name == "StIsFound") {
		gen->SetReturnByte(unit(S(0)) ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "StIsMoving") {
		gen->SetReturnByte(0);
		return DispatchResult::Handled;
	}
	if (name == "StIsConquerFacility") {
		gen->SetReturnByte(0);
		return DispatchResult::Handled;
	}
	if (name == "StGetSkillUseNum") {
		gen->SetReturnDWord(0);
		return DispatchResult::Handled;
	}
	if (name == "StGetUnitNameFoundLast") {
		ret_str(_battle_found_last);
		return DispatchResult::Handled;
	}
	if (name == "StGetPlayerUnitNum") {
		gen->SetReturnDWord(static_cast<asDWORD>(army_count(0)));
		return DispatchResult::Handled;
	}
	if (name == "StGetEnemyUnitNum") {
		int n = 0;
		for (const auto& kv : _battle_units)
			if (!kv.second.vehicle && kv.second.army != 0)
				++n;
		gen->SetReturnDWord(static_cast<asDWORD>(n));
		return DispatchResult::Handled;
	}
	if (name == "StGetUnitNumInCircle") {
		float p[2] = {0.0f, 0.0f};
		args.vec2(1, p);
		const float r = F(2);
		int n = 0;
		for (const auto& kv : _battle_units) {
			if (kv.second.army != I(0) || kv.second.vehicle)
				continue;
			const float dx = kv.second.pos[0] - p[0], dy = kv.second.pos[1] - p[1];
			if (dx * dx + dy * dy <= r * r)
				++n;
		}
		gen->SetReturnDWord(static_cast<asDWORD>(n));
		return DispatchResult::Handled;
	}
	if (name == "StGetUnitObjectName") {
		const int idx = I(0), army = I(1);
		int n = 0;
		std::string res;
		for (const auto& k : _battle_unit_order) {
			auto it = _battle_units.find(k);
			if (it == _battle_units.end() || it->second.vehicle || it->second.army != army)
				continue;
			if (n++ == idx) {
				res = k;
				break;
			}
		}
		ret_str(res);
		return DispatchResult::Handled;
	}
	if (name == "StGetStayingUnitObjectName") {
		std::string res;
		for (const auto& kv : _unit_station)
			if (kv.second == S(0)) {
				res = kv.first;
				break;
			}
		ret_str(res);
		return DispatchResult::Handled;
	}
	if (name == "StGetPartnerCharaDefName") {
		ret_str(_partner_defs.empty() ? std::string() : _partner_defs.front());
		return DispatchResult::Handled;
	}

	// ---- script callback bookkeeping (dispatch deferred) ----------------
	// Stored for TODO(combat)/TODO(field) event dispatch; no callback is invoked
	// until the corresponding game event exists.
	if (name == "AddBattleBefore") {
		add_callback(ScriptEvent::BattleBefore, {}, S(0));
		return DispatchResult::Handled;
	}
	if (name == "AddBattleDie") {
		add_callback(ScriptEvent::BattleDie, S(0), S(1));
		return DispatchResult::Handled;
	}
	if (name == "StAddEncountBefore") {
		add_callback(ScriptEvent::EncounterBefore, {}, S(0));
		return DispatchResult::Handled;
	}
	if (name == "StAddEncountAfter") {
		add_callback(ScriptEvent::EncounterAfter, {}, S(0));
		return DispatchResult::Handled;
	}
	if (name == "StAddTalkEvent") {
		add_callback(ScriptEvent::Talk, S(0) + "\x1f" + S(1), S(2));
		return DispatchResult::Handled;
	}
	if (name == "StDeleteTalkEvent") {
		remove_callback(ScriptEvent::Talk, S(0) + "\x1f" + S(1), S(2));
		return DispatchResult::Handled;
	}
	if (name == "StSetTalkEventMoveBack") {
		_talk_move_back = I(0) != 0;
		return DispatchResult::Handled;
	}
	if (name == "StSetFacilityEvent") {
		add_callback(ScriptEvent::Facility, S(0), S(1), true);
		return DispatchResult::Handled;
	}
	if (name == "StSetGateBreakAfter") {
		add_callback(ScriptEvent::GateBreakAfter, S(0), S(1), true);
		return DispatchResult::Handled;
	}

	// Engine-side unit actions: no state the scripts read back.
	if (name == "StActionEnd" || name == "StActionEnd_AllPlayerArmy" || name == "StUnitWaitSkip" || name == "StEmote" ||
		name == "StSetUnitAnimation" || name == "StSetUnitMov" || name == "StSetMoveSpeedRate" ||
		name == "StSetMoveAiPermissionInDemo" || name == "StSetForcePause" || name == "StDisableBattle" ||
		name == "StSetEncountOff" || name == "StSetNightMask" || name == "StSetMiasumaAllUnit" ||
		name == "StOpenCoAEditor" || name == "StDisableGroundMoveEffect" || name == "StSetFocusReturnPosV" ||
		name == "StSetArmyBase" || name == "StDeleteArmyBase" || name == "StBoard" || name == "StRestartSkipEnd" ||
		name == "SetObjectName") {
		return DispatchResult::Handled;
	}

	// ---- battle scene / messages / timers ------------------------------
	if (name == "StSetBattleScene") {
		// Marks one or two units as being in a battle scene (the engine sets a
		// flag bit on the unit). The turn loop is engine-side.
		const int n = static_cast<int>(gen->GetArgCount());
		_battle_scene = S(0);
		_battle_scene_on = (n >= 3) ? I(2) != 0 : I(1) != 0;
		return DispatchResult::Handled;
	}
	if (name == "StSetBattleBG") {
		_battle_bg = S(0);
		return DispatchResult::Handled;
	}
	if (name == "StCreateTimer") {
		// Countdown timer: the engine stores `restFrame` and decrements it.
		BattleTimer t;
		t.start = frame;
		t.rest = I(1);
		t.running = true;
		_battle_timers[S(0)] = t;
		return DispatchResult::Handled;
	}
	if (name == "StGetTimer") {
		auto it = _battle_timers.find(S(0));
		long left = 0;
		if (it != _battle_timers.end())
			left = std::max(0L, static_cast<long>(it->second.rest) - (frame - it->second.start));
		gen->SetReturnQWord(static_cast<asQWORD>(left));
		return DispatchResult::Handled;
	}
	if (name == "StSetTimeLimit") {
		// Stage clock limit in in-game days/hours/minutes.
		_time_limit_day = I(0);
		_time_limit_hour = I(1);
		_time_limit_minute = I(2);
		return DispatchResult::Handled;
	}
	if (name == "StSetTimeLimitOff") {
		if (I(0)) {
			_time_limit_day = -1;
			_time_limit_hour = -1;
			_time_limit_minute = -1;
		}
		return DispatchResult::Handled;
	}
	if (name == "StResetTimeLimit") {
		_time_limit_day = -1;
		_time_limit_hour = -1;
		_time_limit_minute = -1;
		return DispatchResult::Handled;
	}
	if (name == "StIsTimeLimit") {
		bool over = false;
		if (_time_limit_day >= 0) {
			const int now = _clock_hour * 60 + _clock_minute;
			const int lim = _time_limit_hour * 60 + _time_limit_minute;
			over = now >= lim;
		}
		gen->SetReturnByte(over ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "StGetElapsedClockHour") {
		gen->SetReturnDWord(static_cast<asDWORD>(_clock_hour));
		return DispatchResult::Handled;
	}
	if (name == "StGetElapsedClockMin") {
		gen->SetReturnDWord(static_cast<asDWORD>(_clock_minute));
		return DispatchResult::Handled;
	}
	if (name == "StSetStageTimer") {
		return DispatchResult::Handled;
	} // TODO(phase7): stage timer

	if (name == "StAddWinMessage" || name == "StShowWinMessage" || name == "StAddLoseMessage" ||
		name == "StShowLoseMessage" || name == "StAddWinMessageCommander" || name == "StShowWinMessageCommander" ||
		name == "StClearWinMessage" || name == "StClearLoseMessage" || name == "AddBattleFlag") {
		return DispatchResult::Handled; // battle result/message hooks: engine UI
	}
	if (name == "BtFindChara") {
		bool hit = _party.count(S(0)) != 0;
		for (const auto& kv : _battle_units)
			if (kv.second.def == S(0))
				hit = true;
		gen->SetReturnByte(hit ? 1 : 0);
		return DispatchResult::Handled;
	}

	// ---- AI components / gambit / Zouen --------------------------------
	// Scripts append components to a unit's AI list (move/search/chase/board/
	// conquer); the engine ticks them each turn. A gambit pairs a target
	// selector with a skill and an interval/limit. Zouen is a reinforcement
	// wave spawned by a facility at an interval. All engine-side.
	// TODO(phase7): implement the component list and the turn loop.
	if (name.starts_with("StAddUnitAi") || name == "StClearUnitAi" || name == "StClearUnitAiMove" ||
		name == "StClearMoveTarget" || name == "StHideSearchRange" || name == "StUnitDefaultActionOff" ||
		name == "StUnitDefaultActionSearchOff" || name == "StUnitDefaultActionRevengeOff" ||
		name == "StUnitDefaultActionLookoutOff" || name == "StAddGambitState" || name == "StClearGambitState" ||
		name.starts_with("StSetZouen") || name.starts_with("StAddZouen") || name == "StClearZouenUnitSet" ||
		name == "StCreateZouenSpot" || name == "StCreateKanketusen") {
		return DispatchResult::Handled;
	}

	// ---- facilities / gimmicks / vehicles / regions --------------------
	if (name == "StCreateFacility") {
		_facilities.insert(S(0));
		return DispatchResult::Handled;
	}
	if (name == "StCreateFacility_DefName") {
		_facilities.insert(S(0));
		return DispatchResult::Handled;
	}
	if (name == "StDeleteAllFacility") {
		_facilities.clear();
		return DispatchResult::Handled;
	}
	if (name == "StSetFacilityArmyType") {
		_facility_army[S(0)] = I(1);
		return DispatchResult::Handled;
	}
	if (name == "StGetStayingArmyType") {
		auto it = _facility_army.find(S(0));
		gen->SetReturnDWord(it == _facility_army.end() ? 0u : static_cast<asDWORD>(it->second));
		return DispatchResult::Handled;
	}
	if (name == "StGetBoardingArmyType") {
		gen->SetReturnDWord(0);
		return DispatchResult::Handled;
	}
	if (name == "StStation") {
		// Snaps the unit to the facility's station point and links it into the
		// facility's station list. TODO(phase7): position/list sync.
		_unit_station[S(0)] = S(1);
		return DispatchResult::Handled;
	}
	if (name == "StCreateFacility_StageRect" || name == "StSetFacilityDir" || name == "StSetStationItem" ||
		name == "StDeleteAllGimmick" || name == "StCreateGimmick" || name == "StCreateFire" ||
		name == "StCreateCloud" || name == "StCreateCloudGenerator" || name == "StCreateCircleRegionObject" ||
		name == "StCreateLookoutRegionObject" || name == "StCreateSwitchObject") {
		return DispatchResult::Handled; // record-only / engine object graph
	}

	// ---- selectable / targetable / commands ----------------------------
	if (name == "StAddSelectableObject") {
		_selectable_objects.insert(S(0));
		return DispatchResult::Handled;
	}
	if (name == "StAddTargetableObject") {
		_targetable_objects.insert(S(0));
		return DispatchResult::Handled;
	}
	if (name == "StDeleteAllSelectableObject") {
		_selectable_objects.clear();
		return DispatchResult::Handled;
	}
	if (name == "StDeleteAllTargetableObject") {
		_targetable_objects.clear();
		return DispatchResult::Handled;
	}
	if (name == "StDeleteSelectableObject") {
		_selectable_objects.erase(S(0));
		return DispatchResult::Handled;
	}
	if (name == "StAddSelectableCommand") {
		_selectable_commands.insert(I(0));
		return DispatchResult::Handled;
	}
	if (name == "StDeleteAllSelectableCommand") {
		_selectable_commands.clear();
		return DispatchResult::Handled;
	}
	if (name == "StClearCommandSelected") {
		_selected_command = -1;
		return DispatchResult::Handled;
	}
	if (name == "StIsCommandSelected") {
		gen->SetReturnByte(_selected_command == I(0) ? 1 : 0);
		return DispatchResult::Handled;
	}

	// No spatial simulation yet: these are search-area / polygon tests.
	if (name.starts_with("StIsArmyInSearchArea") || name.starts_with("StIsUnitInSearchArea") ||
		name.starts_with("StIsUnitInRect")) {
		gen->SetReturnByte(0);
		return DispatchResult::Handled;
	}

	// ---- stage rects / map / stage AI ----------------------------------
	if (name == "StDbClearStage") {
		push_field("st_db_clear", S(0), {}, 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "IsVeryHardDie") {
		gen->SetReturnByte(0);
		return DispatchResult::Handled;
	}
	if (name == "DeleteVeryHardDie")
		return DispatchResult::Handled;
	if (name == "StSetStageRect" || name == "StSetStageRectA" || name == "StSetStageMapImageById" ||
		name == "StSetStageMapImageByName" || name == "StSetStageCoatOfArms" || name == "StSetDefaultStageAi" ||
		name == "StSetStageAi" || name == "StSetUnitStageAi" || name == "StSetUnitStageAiConquer" ||
		name == "StLoadNodeNetwork") {
		return DispatchResult::Handled;
	}

	return DispatchResult::Unhandled;
}

} // namespace asb
