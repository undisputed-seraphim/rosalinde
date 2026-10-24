#include "native_args.hpp"
#include "registry.hpp"

#include <algorithm>
#include <cmath>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace asb {

RuntimeField& Registry::push_field(
	std::string kind,
	std::string name,
	std::string text,
	int i0,
	int i1,
	int i2,
	float f0,
	const float* pos,
	bool flag) {
	RuntimeField entry;
	entry.frame = frame;
	entry.kind = std::move(kind);
	entry.name = std::move(name);
	entry.text = std::move(text);
	entry.i0 = i0;
	entry.i1 = i1;
	entry.i2 = i2;
	entry.f0 = f0;
	if (pos) {
		entry.pos[0] = pos[0];
		entry.pos[1] = pos[1];
	}
	entry.flag = flag;
	_timeline.field.push_back(std::move(entry));
	return _timeline.field.back();
}

RuntimeEntity* Registry::field_object(
	const char* kind,
	const std::string& object,
	const float* pos,
	const std::string& mbs,
	const std::string& ftx) {
	if (object.empty())
		return nullptr;
	const int idx = ensure(object);
	auto& e = _timeline.entities[idx];
	e.kind = kind;
	e.space = RuntimeEntity::Space::World;
	alias(idx, object);
	if (!mbs.empty())
		e.mbs = mbs;
	if (!ftx.empty())
		e.ftx = ftx;
	// Field symbols sometimes pass a chara display/label name instead of
	// asset paths (`FlCreateCharaSymbol`/chase variants); resolve through
	// the same CharaSet/display tables as world chara creation.
	auto resolve_name = [&](const std::string& key) {
		if (key.empty())
			return false;
		if (_chara_sets)
			if (const CharaInfo* ci = _chara_sets->resolve(key)) {
				e.mbs = ci->mbs;
				e.ftx = ci->ftx;
				return true;
			}
		if (_chars) {
			auto it = _chars->find(key);
			if (it != _chars->end()) {
				e.mbs = it->second.mbs;
				e.ftx = it->second.ftx;
				return true;
			}
		}
		return false;
	};
	const bool mbs_asset = mbs.find('/') != std::string::npos || mbs.ends_with(".mbs");
	const bool ftx_asset = ftx.find('/') != std::string::npos || ftx.ends_with(".ftx");
	if (!mbs_asset && !resolve_name(mbs) && !ftx_asset)
		resolve_name(ftx);
	if (pos) {
		e.pos[0] = pos[0];
		e.pos[1] = pos[1];
	}
	mark_dirty();
	return &e;
}

DispatchResult Registry::dispatch_field(std::string_view name, NativeArgs& args) {
	asIScriptGeneric* gen = args.generic();
	auto S = [&](int i) { return args.str(i); };
	auto F = [&](int i) { return args.f32(i); };
	auto I = [&](int i) { return args.i32(i); };
	auto V = [&](int i, float* out, int count) {
		if (count == 2)
			args.vec2(i, out);
		else
			args.vec3(i, out);
	};
	auto ent = [&](int i) { return entity_arg(args, i); };

	if (name == "FlIsNightTime") {
		gen->SetReturnByte(0);
		return DispatchResult::Handled;
	}

	// Field state / clock / weather.
	if (name == "FlSetState") {
		if (auto* e = ent(0)) {
			e->state = I(1);
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlGetState") {
		auto* e = ent(0);
		gen->SetReturnDWord(e ? static_cast<asDWORD>(e->state) : 0);
		return DispatchResult::Handled;
	}
	if (name == "FlAddState") {
		if (auto* e = ent(0)) {
			e->state |= I(1);
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlSetStateIcon") {
		if (auto* e = ent(0)) {
			e->state_icon = I(1);
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlClearStateIcon") {
		if (auto* e = ent(0)) {
			e->state_icon = 0;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlSetClock") {
		_clock_hour = I(0);
		_clock_minute = I(1);
		_clock_second = I(2);
		push_field("clock", {}, {}, _clock_hour, _clock_minute, _clock_second, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlSetClockHour") {
		_clock_hour = I(0);
		push_field("clock", {}, {}, _clock_hour, _clock_minute, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlSetClockMinute") {
		_clock_minute = I(0);
		push_field("clock", {}, {}, _clock_hour, _clock_minute, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlGetClockHour") {
		gen->SetReturnDWord(static_cast<asDWORD>(_clock_hour));
		return DispatchResult::Handled;
	}
	if (name == "FlGetClockMinute") {
		gen->SetReturnDWord(static_cast<asDWORD>(_clock_minute));
		return DispatchResult::Handled;
	}
	if (name == "FlPauseClock") {
		_clock_paused = I(0) != 0;
		push_field("clock_pause", {}, {}, _clock_paused ? 1 : 0, 0, 0, 0.0f, nullptr, _clock_paused);
		return DispatchResult::Handled;
	}
	if (name == "FlSetupWeatherByCamera") {
		push_field("weather_camera", {}, {}, 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlAddWeatherRegion") {
		auto pts = args.ints(0);
		push_field("weather_region", {}, S(2), static_cast<int>(pts.size()), I(1), 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlDelWeatherRegion") {
		push_field("weather_region_del", S(0), {}, 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlWeatherFade") {
		push_field("weather_fade", {}, {}, I(0), I(1), 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "SetFieldStageAllEnvSE") {
		push_field("field_env_se", {}, {}, I(0), 0, 0, 0.0f, nullptr, I(0) != 0);
		return DispatchResult::Handled;
	}

	// Symbols.
	if (name == "FlCreateCharaSymbol") {
		float p[2] = {0.0f, 0.0f};
		V(3, p, 2);
		field_object("FlCreateCharaSymbol", S(0), p, S(1), S(2));
		return DispatchResult::Handled;
	}
	if (name == "FlGetPlayerSymbolName") {
		if (auto* out = static_cast<std::string*>(gen->GetAddressOfReturnLocation()))
			new (out) std::string(_player_symbol);
		return DispatchResult::Handled;
	}
	if (name == "FlGetPlayerSymbolPosV") {
		float* r = static_cast<float*>(gen->GetAddressOfReturnLocation());
		if (r) {
			r[0] = r[1] = 0.0f;
			auto it = _by_name.find(_player_symbol);
			if (it != _by_name.end()) {
				r[0] = _timeline.entities[it->second].pos[0];
				r[1] = _timeline.entities[it->second].pos[1];
			}
		}
		return DispatchResult::Handled;
	}
	if (name == "FlChangeCharaSymbolPlayer") {
		_player_symbol = S(0);
		return DispatchResult::Handled;
	}
	if (name == "FlBackHomeSymbol") {
		push_field("back_home", S(0), {}, 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlPauseSymbol") {
		if (auto* e = ent(0)) {
			e->paused = true;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlUnpauseSymbol") {
		if (auto* e = ent(0)) {
			e->paused = false;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	// TODO(field): auto-facing on movement is not simulated; state is retained.
	if (name == "FlSetAutoDirOff") {
		if (auto* e = ent(0)) {
			e->auto_dir_off = I(1) != 0;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlSetAnimation") {
		if (auto* e = ent(0)) {
			set_animation_index(*e, I(1));
			e->reverse = I(2) != 0;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlSetReverseX") {
		if (auto* e = ent(0)) {
			e->reverse_x = I(1) != 0;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlSetHeight") {
		if (auto* e = ent(0)) {
			e->height = F(1);
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlEmote") {
		if (auto* e = ent(0)) {
			e->emote = I(1);
			mark_dirty();
		}
		push_field("emote", S(0), {}, I(1), I(2), 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlIsSymbolInCircle") {
		float p[2] = {0.0f, 0.0f};
		V(1, p, 2);
		bool inside = false;
		if (auto* e = ent(0)) {
			const float dx = e->pos[0] - p[0], dy = e->pos[1] - p[1];
			const float r = static_cast<float>(I(2));
			inside = dx * dx + dy * dy <= r * r;
		}
		gen->SetReturnByte(inside ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "FlAddSymbolAiMove") {
		// TODO(field-ai): one-shot tween; the engine's symbol AI is persistent.
		float p[2] = {0.0f, 0.0f};
		V(1, p, 2);
		if (auto* e = ent(0)) {
			Move m{};
			for (int k = 0; k < 3; ++k) {
				m.from[k] = e->pos[k];
				m.to[k] = e->pos[k];
			}
			m.to[0] = p[0];
			m.to[1] = p[1];
			const float dist = std::hypot(m.to[0] - m.from[0], m.to[1] - m.from[1]);
			m.f0 = frame;
			m.f1 = frame + std::max(1, static_cast<int>(std::ceil(dist / 2.0f)));
			_moves[e->name] = m;
			mark_dirty();
		}
		push_field("symbol_move", S(0), {}, 0, 0, 0, 0.0f, p, false);
		return DispatchResult::Handled;
	}
	if (name == "FlAddSymbolAiMoveWait") {
		push_field("symbol_wait", S(0), {}, I(1), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlAddSymbolAiMoveTurn") {
		if (auto* e = ent(0)) {
			e->dir_x = I(1) >= 0 ? 1.0f : -1.0f;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlSetMoveAiPatRolType") {
		push_field("symbol_ai", S(0), {}, I(1), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	// TODO(field): consume ai_paused once field AI/movement exists.
	if (name == "FlSetMoveAiPermissionInDemo") {
		if (auto* e = ent(0)) {
			e->ai_paused = I(1) == 0;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlSetDemoMoveSpeed") {
		push_field("demo_speed", S(0), {}, I(1), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}

	// Field objects / regions / portals.
	if (name == "FlCreateEnemy") {
		float p[2] = {0.0f, 0.0f};
		V(3, p, 2);
		field_object("FlCreateEnemy", S(0), p, S(1));
		return DispatchResult::Handled;
	}
	if (name == "FlCreateChaseEnemy") {
		float p[2] = {0.0f, 0.0f};
		V(2, p, 2);
		field_object("FlCreateChaseEnemy", S(0), p);
		push_field("chase_enemy", S(0), S(5), I(1), I(3), I(4), 0.0f, p, false);
		add_callback(ScriptEvent::Chase, S(0), S(5), true);
		return DispatchResult::Handled;
	}
	if (name == "FlCreateChaseChara" || name == "FlCreateFleeChara") {
		float p[2] = {0.0f, 0.0f};
		V(3, p, 2);
		field_object(name == "FlCreateChaseChara" ? "FlCreateChaseChara" : "FlCreateFleeChara", S(0), p, S(1), S(2));
		return DispatchResult::Handled;
	}
	if (name == "FlDeleteEnemy" || name == "FlDelRegionChaseEnemy" || name == "FlDeleteTalkPoint") {
		if (auto* e = ent(0)) {
			e->visible = false;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlDeleteEnemyNotify")
		return DispatchResult::Handled;
	if (name == "FlCreateVehicle") {
		float p[2] = {0.0f, 0.0f};
		V(2, p, 2);
		field_object("FlCreateVehicle", S(0), p);
		return DispatchResult::Handled;
	}
	if (name == "FlCreateCollision") {
		auto pts = args.ints(1);
		push_field("collision", S(0), {}, static_cast<int>(pts.size()), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlDeleteCollision") {
		push_field("collision_del", S(0), {}, 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlRebuildAreaCollision" || name == "FlRebuildAreaCollisionWait" ||
		name == "FlRebuildAreaCollisionAsync")
		return DispatchResult::Handled;
	if (name == "FlCreatePortal") {
		float a[2] = {0.0f, 0.0f}, b[2] = {0.0f, 0.0f};
		V(1, a, 2);
		V(2, b, 2);
		RuntimeField& f = push_field("portal", S(0), S(5), I(3), I(4), 0, 0.0f, a, false);
		f.i1 = I(4);
		f.text2 = S(6);
		f.pos[0] = b[0];
		f.pos[1] = b[1];
		add_callback(ScriptEvent::Portal, S(0), S(5), true, "0");
		add_callback(ScriptEvent::Portal, S(0), S(6), true, "1");
		return DispatchResult::Handled;
	}
	if (name == "FlCreateTalkPoint") {
		float p[2];
		V(1, p, 2);
		push_field("talk_point", S(0), {}, I(2), 0, 0, 0.0f, p, false);
		return DispatchResult::Handled;
	}
	if (name == "FlCreateSearchPoint") {
		float p[2];
		V(2, p, 2);
		push_field("search_point", S(0), S(1), 0, 0, 0, F(3), p, false);
		return DispatchResult::Handled;
	}
	if (name == "FlCreateGatheringSpot") {
		float p[2];
		V(1, p, 2);
		push_field("gathering", S(0), S(3), I(2), 0, 0, 0.0f, p, false);
		return DispatchResult::Handled;
	}
	if (name == "FlCreateBonFire") {
		float p[2];
		V(1, p, 2);
		push_field("bonfire", S(0), {}, 0, 0, 0, 0.0f, p, false);
		return DispatchResult::Handled;
	}
	if (name == "FlCreateFreeMap") {
		float p[2];
		V(1, p, 2);
		push_field("free_map", S(0), S(2), I(3), 0, 0, 0.0f, p, false);
		return DispatchResult::Handled;
	}
	if (name == "FlCreateStage") {
		push_field("stage", S(0), {}, I(1), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlCreateExStage") {
		float p[2];
		V(1, p, 2);
		push_field("ex_stage", S(0), S(2), I(3), 0, 0, 0.0f, p, false);
		return DispatchResult::Handled;
	}
	if (name == "FlAddPort") {
		push_field("port", S(0), S(1), I(2), 0, 0, 0.0f, nullptr, false).text2 = S(3);
		add_callback(ScriptEvent::Port, S(0), S(3), true);
		return DispatchResult::Handled;
	}
	if (name == "FlSetRevivePosV") {
		float p[2];
		V(0, p, 2);
		_revive_pos[0] = p[0];
		_revive_pos[1] = p[1];
		return DispatchResult::Handled;
	}
	if (name == "FlDeleteMarker") {
		push_field("marker_del", S(0), {}, 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlCreateGradientMapRegion") {
		auto pts = args.ints(1);
		push_field("gradient_region", S(0), {}, static_cast<int>(pts.size()), I(2), 0, F(3), nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlDeleteGradientMapRegion") {
		push_field("gradient_region_del", S(0), {}, 0, 0, 0, 0.0f, nullptr, I(1) != 0);
		return DispatchResult::Handled;
	}
	if (name == "FlSetGradientMap") {
		push_field("gradient", {}, {}, I(0), I(2), 0, F(1), nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "DrCreateDrama") {
		push_field("drama", S(0), {}, 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "ReconstTown")
		return DispatchResult::Handled;
	if (name == "FlRemoveDeliveryCamera") {
		push_field("delivery_camera", {}, {}, I(0), 0, 0, 0.0f, nullptr, I(0) != 0);
		return DispatchResult::Handled;
	}
	if (name == "FlStartDigGame") {
		float p[2];
		V(0, p, 2);
		push_field("dig_game", S(1), S(2), 0, 0, 0, 0.0f, p, I(3) != 0);
		return DispatchResult::Handled;
	}
	if (name == "FlCatchChicken") {
		push_field("chicken", {}, {}, I(0), 0, 0, 0.0f, nullptr, I(0) != 0);
		return DispatchResult::Handled;
	}

	// Field flags / UI.
	if (name == "FlSetDemoMode") {
		_demo_mode = I(0) != 0;
		push_field("demo_mode", {}, {}, _demo_mode ? 1 : 0, 0, 0, 0.0f, nullptr, _demo_mode);
		return DispatchResult::Handled;
	}
	if (name == "FlSetBattleMode") {
		_battle_mode = I(1) != 0;
		push_field("battle_mode", S(0), {}, _battle_mode ? 1 : 0, 0, 0, 0.0f, nullptr, _battle_mode);
		return DispatchResult::Handled;
	}
	if (name == "FlSetBattleAfter") {
		_battle_after = S(1);
		add_callback(ScriptEvent::BattleAfter, S(0), S(1), true);
		push_field("battle_after", S(0), S(1), 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlSetBattleDieAfter") {
		_battle_die_after = S(1);
		add_callback(ScriptEvent::BattleDieAfter, S(0), S(1), true);
		push_field("battle_die_after", S(0), S(1), 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlDisableWorldMap") {
		_world_map_disabled = I(0) != 0;
		push_field("world_map", {}, {}, _world_map_disabled ? 1 : 0, 0, 0, 0.0f, nullptr, _world_map_disabled);
		return DispatchResult::Handled;
	}
	if (name == "FlSetDispGrid") {
		push_field("disp_grid", {}, {}, I(0), I(1), I(2), static_cast<float>(I(3)), nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlSetChipClip") {
		push_field("chip_clip", {}, {}, I(0), 0, 0, 0.0f, nullptr, I(0) != 0);
		return DispatchResult::Handled;
	}
	if (name == "FlSetScreenFilterDisplay") {
		push_field("screen_filter", S(0), {}, I(2), 0, 0, 0.0f, nullptr, I(1) != 0);
		return DispatchResult::Handled;
	}
	if (name == "FlSetAreaObjectDisplay") {
		push_field("area_object", S(0), {}, I(2), 0, 0, 0.0f, nullptr, I(1) != 0);
		return DispatchResult::Handled;
	}
	if (name == "FlSetAreaObjectAnimeOffset") {
		push_field("area_anime", S(0), {}, I(1), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlSetAreaCollisionEnabled") {
		push_field("area_collision", S(0), {}, 0, 0, 0, 0.0f, nullptr, I(1) != 0);
		return DispatchResult::Handled;
	}
	if (name == "FlSetGateOpenSettingToAreaCollision") {
		push_field("gate_open", S(0), {}, 0, 0, 0, 0.0f, nullptr, I(1) != 0);
		return DispatchResult::Handled;
	}
	if (name == "FlSetFacilityAfter") {
		add_callback(ScriptEvent::FacilityAfter, S(0), S(1), true);
		push_field("facility_after", S(0), S(1), 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlSetFacilityDisable") {
		push_field("facility_disable", S(0), {}, 0, 0, 0, 0.0f, nullptr, I(1) != 0);
		return DispatchResult::Handled;
	}
	if (name == "FlSetSearchFlag") {
		if (auto* e = ent(0)) {
			e->search_flag = I(1) != 0;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "FlOnlyOneTownLvMax") {
		_town_lv[S(0)] = 99;
		push_field("town_max", S(0), {}, 99, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlAllTownLvMax") {
		for (auto& [k, v] : _town_lv)
			v = 99;
		push_field("town_max_all", {}, {}, 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlAddTownLvUpEvent") {
		add_callback(ScriptEvent::TownLvUp, S(0), S(1));
		push_field("town_lv_event", S(0), S(1), 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlSetStageStartParam") {
		_stage_start_param = I(1);
		push_field("stage_start", S(0), {}, I(1), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	// TODO(field): map-completion formula is not recovered yet.
	if (name == "FlGetBreakthroughRate") {
		gen->SetReturnFloat(0.0f);
		return DispatchResult::Handled;
	}
	if (name == "FlGetTownLv") {
		auto it = _town_lv.find(S(0));
		gen->SetReturnDWord(it == _town_lv.end() ? 0u : static_cast<asDWORD>(it->second));
		return DispatchResult::Handled;
	}
	if (name == "FlGetItemChangeShopLevel") {
		gen->SetReturnDWord(static_cast<asDWORD>(_item_change_shop_level));
		return DispatchResult::Handled;
	}
	if (name == "FlGetItemChangeNum") {
		gen->SetReturnDWord(static_cast<asDWORD>(_item_change_num));
		return DispatchResult::Handled;
	}
	if (name == "FlOpenItemChangeShop") {
		push_field("item_shop", {}, {}, 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "FlGetResultExStageStartDialog") {
		gen->SetReturnDWord(static_cast<asDWORD>(_ex_stage_dialog_result));
		return DispatchResult::Handled;
	}
	if (name == "FlOpenExStageStartDialog") {
		push_field("ex_stage_dialog", {}, S(1), I(0), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "GetFacilityFieldPosV") {
		float* r = static_cast<float*>(gen->GetAddressOfReturnLocation());
		if (r) {
			r[0] = r[1] = 0.0f;
			if (auto* e = ent(0)) {
				r[0] = e->pos[0];
				r[1] = e->pos[1];
			}
		}
		return DispatchResult::Handled;
	}

	// Data getters (existing tables).
	if (name == "GetCharaDefNameByCharaSet") {
		std::string res;
		if (_chara_sets)
			res = _chara_sets->name_at(I(0));
		if (auto* out = static_cast<std::string*>(gen->GetAddressOfReturnLocation()))
			new (out) std::string(res);
		return DispatchResult::Handled;
	}
	if (name == "GetCharaNameByCharaDefName") {
		std::string res = S(0);
		if (_chara_sets)
			if (const CharaInfo* ci = _chara_sets->resolve(S(0)))
				res = ci->disp_name;
		if (auto* out = static_cast<std::string*>(gen->GetAddressOfReturnLocation()))
			new (out) std::string(res);
		return DispatchResult::Handled;
	}
	if (name == "GetStationFacilityName") {
		// The argument is an object name: a unit resolves through the facility it
		// is stationed at (StStation), otherwise it is treated as the facility.
		std::string key = S(0);
		if (auto it = _unit_station.find(key); it != _unit_station.end())
			key = it->second;
		std::string res;
		if (_facility_name)
			res = _facility_name(key);
		if (res.empty())
			res = key;
		if (auto* out = static_cast<std::string*>(gen->GetAddressOfReturnLocation()))
			new (out) std::string(std::move(res));
		return DispatchResult::Handled;
	}
	// Stage/facility data from the engine `.data` dump.
	if (name == "GetStageBaseLevel") {
		const auto* s = _stages ? _stages->stage(S(0)) : nullptr;
		gen->SetReturnDWord(s ? static_cast<asDWORD>(s->base_lv) : 0u);
		return DispatchResult::Handled;
	}
	if (name == "GetStageCueId") {
		const auto* s = _stages ? _stages->stage(S(0)) : nullptr;
		gen->SetReturnDWord(s && s->cue >= 0 ? static_cast<asDWORD>(s->cue) : 0u);
		return DispatchResult::Handled;
	}
	if (name == "GetStagePos") {
		float* r = static_cast<float*>(gen->GetAddressOfReturnLocation());
		const auto* f = _stages ? _stages->facility(S(0)) : nullptr;
		if (r) {
			r[0] = f ? f->pos[0] : 0.0f;
			r[1] = f ? f->pos[1] : 0.0f;
		}
		return DispatchResult::Handled;
	}
	if (name == "GetWinIDByFlBGLane") {
		// Hardcoded in the engine: lanes 0..3 map to 0x116/0x117/0x118/0x11a.
		static const int ids[4] = {0x116, 0x117, 0x118, 0x11a};
		const int lane = I(0);
		gen->SetReturnDWord(lane >= 0 && lane < 4 ? static_cast<asDWORD>(ids[lane]) : 0xffffffffu);
		return DispatchResult::Handled;
	}
	if (name == "GetFieldCueId") {
		// Engine 0x2f8620: a string-length switch over the nation name; the four
		// explicit names map to nation ids, everything else (lengths other than
		// 12/15/18/21) to nation 1. Nation id -> (day, night) cue comes from the
		// engine table at 0x2400478 (stride 0x14, fields +4/+8).
		const std::string& n = S(0);
		const bool night = gen->GetArgCount() > 1 && I(1) != 0;
		int cue = 0;
		switch (n.size()) {
		case 12:
			cue = n == "ゼノイラ" ? (night ? 111 : 110) : 0;
			break;
		case 15:
			cue = n == "エルヘイム" ? 112 : 0;
			break;
		case 18:
			cue = n == "バストリアス" ? (night ? 107 : 106) : 0;
			break;
		case 21:
			cue = n == "ドラケンガルド" ? (night ? 109 : 108) : 0;
			break;
		default:
			cue = night ? 105 : 104;
			break;
		}
		gen->SetReturnDWord(static_cast<asDWORD>(cue));
		return DispatchResult::Handled;
	}
	if (name == "GetCountryByPos") {
		float p[2] = {0.0f, 0.0f};
		V(0, p, 2);
		gen->SetReturnDWord(_stages ? static_cast<asDWORD>(_stages->country_at(p)) : 0u);
		return DispatchResult::Handled;
	}

	return DispatchResult::Unhandled;
}

} // namespace asb
