#include "registry.hpp"

#include "native_args.hpp"

#include <angelscript.h>
#include <scriptarray/scriptarray.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string_view>

namespace asb {

namespace {

void read_vec(asIScriptGeneric* gen, int i, float* out, int n) {
	auto* p = static_cast<const float*>(gen->GetArgAddress(i));
	for (int k = 0; k < n; ++k)
		out[k] = p ? p[k] : 0.0f;
}

bool plausible_scale(float s) { return std::isfinite(s) && s > 1e-4f && s < 1e4f; }

float camera_tween_progress(long elapsed, long duration, float ease_in, float ease_out) {
	if (duration <= 0 || elapsed >= duration)
		return 1.0f;
	if (elapsed <= 0)
		return 0.0f;
	const float d = static_cast<float>(duration);
	const float in = std::clamp(ease_in, 0.0f, d);
	const float out = std::clamp(ease_out, 0.0f, d);
	const float denom = 2.0f * d - in - out;
	if (denom <= 0.0f)
		return static_cast<float>(elapsed) / d;
	const float scale = 1.0f / denom;
	if (in > 0.0f && static_cast<float>(elapsed) <= in)
		return std::min(1.0f, scale * static_cast<float>(elapsed) * static_cast<float>(elapsed) / in);
	const float decel_start = d - out;
	if (static_cast<float>(elapsed) <= decel_start)
		return std::min(1.0f, scale * (2.0f * static_cast<float>(elapsed) - in));
	if (out <= 0.0f)
		return 1.0f;
	const float remaining = d - static_cast<float>(elapsed);
	return std::min(1.0f, scale * (2.0f * d - in - out - remaining * remaining / out));
}

} // namespace

void Registry::reset(std::string source, std::string function) {
	_timeline = Timeline{};
	_timeline.source = std::move(source);
	_timeline.function = std::move(function);
	_timeline.mode = _mode;
	_by_name.clear();
	_unit_dir.clear();
	_moves.clear();
	_animations.reset();
	_db_params.clear();
	_help.clear();
	_balloons.clear();
	_sprite_texts.clear();
	_balloon_owner.clear();
	_sprite_text_owner.clear();
	_opaque_owner.clear();
	_select_result = 0;
	_sprite_text_handle = 0;
	_variation = 0;
	_skip_mode = false;
	_camera_move_end = 0;
	_timeline.camera = RuntimeCamera{};
	_shake_seq = 0;
	_follow_offset[0] = _follow_offset[1] = 0.0f;
	_partner_select_open = false;
	_disable_backlog = false;
	_disable_option = false;
	_title_library = false;
	_class_pedia_open = false;
	_all_class_pedia_bg = false;
	_add_class_pedia_flag = false;
	_staff_roll_scroll = false;
	_staff_roll_speed = 0.0f;
	_demo_mode = false;
	_world_map_disabled = false;
	_battle_mode = false;
	_battle_after.clear();
	_battle_die_after.clear();
	_player_symbol.clear();
	_revive_pos[0] = _revive_pos[1] = 0.0f;
	_item_change_shop_level = 0;
	_item_change_num = 0;
	_ex_stage_dialog_result = 0;
	_stage_start_param = 0;
	_battle_units.clear();
	_battle_timers.clear();
	_unit_station.clear();
	_facility_army.clear();
	_selectable_objects.clear();
	_targetable_objects.clear();
	_selectable_commands.clear();
	_selected_command = -1;
	_battle_scene.clear();
	_battle_bg.clear();
	_battle_scene_on = false;
	_time_limit_day = _time_limit_hour = _time_limit_minute = -1;
	_stage_unit_base_lv = 0;
	_battle_brave = 0;
	_battle_found_last.clear();
	_partner_defs.clear();
	_battle_unit_order.clear();
	_dirty = false;
	_talk_move_back = false;
	frame = 0;
	pending_wait = -1;
}

RuntimeEntity* Registry::entity_arg(const NativeArgs& args, int index) {
	auto it = _by_name.find(args.str(index));
	return it == _by_name.end() ? nullptr : &_timeline.entities[it->second];
}

void Registry::set_animation(RuntimeEntity& entity, const std::string& track) {
	int track_index = -1;
	if (!track.empty() && _resolve_track)
		track_index = _resolve_track(entity.mbs, track);
	_animations.play_idle(entity, track, track_index, frame);
}

void Registry::set_animation_index(RuntimeEntity& entity, int track_index) {
	_animations.play_index(entity, track_index, frame);
}

const char* Registry::callback_event_name(ScriptEvent event) {
	switch (event) {
	case ScriptEvent::BattleBefore:
		return "battle_before";
	case ScriptEvent::BattleDie:
		return "battle_die";
	case ScriptEvent::EncounterBefore:
		return "encounter_before";
	case ScriptEvent::EncounterAfter:
		return "encounter_after";
	case ScriptEvent::Talk:
		return "talk";
	case ScriptEvent::Facility:
		return "facility";
	case ScriptEvent::GateBreakAfter:
		return "gate_break_after";
	case ScriptEvent::TownLvUp:
		return "town_lv_up";
	case ScriptEvent::BattleAfter:
		return "battle_after";
	case ScriptEvent::BattleDieAfter:
		return "battle_die_after";
	case ScriptEvent::FacilityAfter:
		return "facility_after";
	case ScriptEvent::Treasure:
		return "treasure";
	case ScriptEvent::Chase:
		return "chase";
	case ScriptEvent::Portal:
		return "portal";
	case ScriptEvent::Port:
		return "port";
	}
	return "unknown";
}

void Registry::add_callback(ScriptEvent event, std::string key, std::string function, bool replace, std::string slot) {
	if (function.empty())
		return;
	const char* name = callback_event_name(event);
	if (replace) {
		_timeline.callbacks.erase(
			std::remove_if(
				_timeline.callbacks.begin(),
				_timeline.callbacks.end(),
				[&](const RuntimeCallback& cb) { return cb.event == name && cb.key == key && cb.slot == slot; }),
			_timeline.callbacks.end());
	}
	_timeline.callbacks.push_back(RuntimeCallback{frame, name, std::move(key), std::move(slot), std::move(function)});
}

void Registry::remove_callback(
	ScriptEvent event,
	const std::string& key,
	const std::string& function,
	const std::string& slot) {
	const char* name = callback_event_name(event);
	_timeline.callbacks.erase(
		std::remove_if(
			_timeline.callbacks.begin(),
			_timeline.callbacks.end(),
			[&](const RuntimeCallback& cb) {
				return cb.event == name && cb.key == key && (function.empty() || cb.function == function) &&
					   (slot.empty() || cb.slot == slot);
			}),
		_timeline.callbacks.end());
}

asIScriptFunction* Registry::resolve_callback(std::string_view name) const {
	if (!_module || name.empty())
		return nullptr;
	asIScriptFunction* bare = nullptr;
	for (asUINT i = 0; i < _module->GetFunctionCount(); ++i) {
		asIScriptFunction* fn = _module->GetFunctionByIndex(i);
		if (!fn)
			continue;
		const char* ns = fn->GetNamespace();
		const char* fn_name = fn->GetName();
		if (!fn_name)
			continue;
		std::string full = (ns && *ns) ? std::string(ns) + "::" + fn_name : std::string(fn_name);
		if (name == full)
			return fn;
		if (name == fn_name) {
			if (bare)
				bare = nullptr; // ambiguous bare name; require the qualified form
			else
				bare = fn;
		}
	}
	return bare;
}

bool Registry::fire_script_event(ScriptEvent event, std::string_view key) {
	// TODO(combat): invoke BattleBefore/BattleDie/Encounter*/BattleAfter/
	// BattleDieAfter callbacks when the battle simulation emits these events.
	// TODO(field): invoke Talk/Facility/GateBreakAfter/TownLvUp/FacilityAfter/
	// Treasure/Chase/Portal/Port callbacks when field interaction exists.
	// Callback storage/removal is complete; dispatch needs a coroutine-capable
	// invoker and real event sources.
	const char* name = callback_event_name(event);
	for (const auto& cb : _timeline.callbacks) {
		if (cb.event != name || (!key.empty() && cb.key != key))
			continue;
		if (std::getenv("ASB_CALLBACKS")) {
			const bool combat = event == ScriptEvent::BattleBefore || event == ScriptEvent::BattleDie ||
								event == ScriptEvent::EncounterBefore || event == ScriptEvent::EncounterAfter ||
								event == ScriptEvent::BattleAfter || event == ScriptEvent::BattleDieAfter;
			std::fprintf(
				stderr,
				"[callback] %s key=\"%s\"%s%s -> %s (not fired: TODO(%s))\n",
				name,
				cb.key.c_str(),
				cb.slot.empty() ? "" : " slot=",
				cb.slot.c_str(),
				cb.function.c_str(),
				combat ? "combat" : "field");
		}
	}
	return false;
}

int Registry::ensure(const std::string& name) {
	auto it = _by_name.find(name);
	if (it != _by_name.end())
		return it->second;
	int idx = static_cast<int>(_timeline.entities.size());
	RuntimeEntity e;
	e.name = name;
	e.order = idx;
	_timeline.entities.push_back(std::move(e));
	if (!name.empty())
		_by_name[name] = idx;
	return idx;
}

void Registry::alias(int idx, const std::string& name) {
	if (!name.empty())
		_by_name[name] = idx;
}

void Registry::create_sprite(
	const std::string& kind,
	const std::string& id,
	std::string mbs,
	std::string ftx,
	const float* pos3,
	int layer,
	float scale) {
	trace_event(
		"sprite",
		"create",
		"\"kind\":\"" + trace::quote(kind) + "\",\"name\":\"" + trace::quote(id) + "\",\"mbs\":\"" + trace::quote(mbs) +
			"\",\"ftx\":\"" + trace::quote(ftx) + "\",\"layer\":" + trace::integer(layer) +
			",\"scale\":" + trace::number(scale) + ",\"pos\":" + trace::vec3(pos3));
	int idx = ensure(id);
	auto& e = _timeline.entities[idx];
	e.kind = kind;
	// CreateSpriteMone is the 2D overlay variant: its coordinates are design-space
	// screen coordinates, not field coordinates. The World/StageStart variants
	// stay in field space.
	e.space = kind == "CreateSpriteMone" ? RuntimeEntity::Space::Ui : RuntimeEntity::Space::World;
	e.started = false;
	if (!id.empty() && e.aliases.empty())
		e.aliases.push_back(id);
	if (!mbs.empty())
		e.mbs = std::move(mbs);
	if (!ftx.empty())
		e.ftx = std::move(ftx);
	e.pos[0] = pos3[0];
	e.pos[1] = pos3[1];
	e.pos[2] = pos3[2];
	e.layer = layer;
	if (plausible_scale(scale))
		e.scale = scale;
	mark_dirty();
}

void Registry::create_chara(const std::string& kind, const std::vector<std::string>& names) {
	std::string id = names.size() >= 2 ? names[1] : (names.empty() ? std::string() : names[0]);
	if (id.empty() && !names.empty())
		id = names[0];
	{
		std::string aliases;
		for (const auto& n : names)
			aliases += (aliases.empty() ? "" : ",") + ("\"" + trace::quote(n) + "\"");
		trace_event(
			"sprite",
			"chara",
			"\"kind\":\"" + trace::quote(kind) + "\",\"name\":\"" + trace::quote(id) + "\",\"aliases\":[" + aliases +
				"]");
	}
	int idx = ensure(id);
	auto& e = _timeline.entities[idx];
	e.kind = kind;
	e.space = kind == "DrUiCreateChara" ? RuntimeEntity::Space::Ui : RuntimeEntity::Space::World;
	e.started = true;
	// UI chara are created hidden and faded in later by DrUiSetColorA; world
	// chara (DrCreateChara/Npc) are visible immediately.
	if (e.space == RuntimeEntity::Space::Ui)
		e.alpha = 0;
	e.aliases = names;
	e.appearance_variant = 0xFFFFFFFFu;
	e.appearance_mask = 0;
	// DrUiCreateChara carries the asset pair explicitly (ftx, mbs, label,
	// chara-set name). World characters instead resolve through the CharaSet
	// appearance table; keep that path separate.
	if (kind == "DrUiCreateChara") {
		if (names.size() > 3) {
			e.mbs = names[2];
			e.ftx = names[3];
		}
	}
	if (_chara_sets) {
		for (const auto& n : names) {
			if (n.empty())
				continue;
			if (const CharaInfo* ci = _chara_sets->resolve(n)) {
				if (e.mbs.empty()) {
					e.mbs = ci->mbs;
					e.ftx = ci->ftx;
				}
				e.appearance_variant = ci->appearance_variant;
				e.appearance_mask = ci->appearance_mask;
				break;
			}
		}
	}
	if (e.mbs.empty() && _chars) {
		auto it = _chars->find(id);
		if (it == _chars->end() && !names.empty())
			it = _chars->find(names[0]);
		if (it != _chars->end()) {
			e.mbs = it->second.mbs;
			e.ftx = it->second.ftx;
			e.appearance_variant = it->second.appearance_variant;
			e.appearance_mask = it->second.appearance_mask;
		}
	}
	for (const auto& n : names)
		alias(idx, n);
	mark_dirty();
}

bool Registry::construct_value(asIScriptGeneric* gen) {
	asIScriptFunction* fn = gen->GetFunction();
	if (!fn)
		return false;
	// Factories for the engine's opaque reference types ($beh3 / $fact) must
	// return a real handle: a null handle is what a script dereferences. Allocate
	// the shim object and hand its pointer back (the setters key off it).
	const std::string_view fname = fn->GetName() ? fn->GetName() : "";
	if (fname == "$beh3" || fname == "$fact") {
		asITypeInfo* rt = gen->GetEngine()->GetTypeInfoById(fn->GetReturnTypeId() & ~asTYPEID_OBJHANDLE);
		const std::string rn = (rt && rt->GetName()) ? rt->GetName() : "";
		if (rn == "array") {
			const int argc = static_cast<int>(gen->GetArgCount());
			const int first = argc >= 2 ? 1 : 0; // arg 0 is the `int&in` type id when present
			const asUINT length = gen->GetArgDWord(first);
			const void* fill = argc > first + 1 ? gen->GetArgAddress(first + 1) : nullptr;
			asITypeInfo* arr = (rt && rt->GetSubType()) ? rt : nullptr;
			if (!arr) {
				// The factory's return type is the array *template*; these scripts
				// build array<int>, so recover the concrete instantiation.
				arr = gen->GetEngine()->GetTypeInfoByDecl("array<int>");
			}
			if (arr) {
				CScriptArray* a = fill ? CScriptArray::Create(arr, length, const_cast<void*>(fill))
									   : CScriptArray::Create(arr, length);
				if (a) {
					gen->SetReturnObject(a);
					return true;
				}
			}
			return false;
		}
		if (rn == "UcMsgBalloonParam") {
			auto p = std::make_unique<BalloonParam>();
			void* raw = p.get();
			_balloon_owner.push_back(std::move(p));
			_balloons[raw] = BalloonParam{};
			gen->SetReturnObject(raw);
			return true;
		}
		if (rn == "UcSpriteTextParam") {
			auto p = std::make_unique<SpriteTextParam>();
			void* raw = p.get();
			_sprite_text_owner.push_back(std::move(p));
			_sprite_texts[raw] = SpriteTextParam{};
			gen->SetReturnObject(raw);
			return true;
		}
		// Any other engine reference type (UcStObject, UcStChaseParam, ...):
		// hand out a zeroed opaque object rather than a null handle, which scripts
		// null-check (and crash on) after the factory call.
		if (rt) {
			auto p = std::make_unique<char[]>(256);
			std::memset(p.get(), 0, 256);
			void* raw = p.get();
			gen->SetReturnObject(raw);
			_opaque_owner.push_back(std::move(p));
			return true;
		}
		return false;
	}
	asITypeInfo* ot = fn->GetObjectType();
	void* obj = gen->GetObject();
	if (!ot || !obj)
		return false;
	std::string on = ot->GetName();
	const int n = static_cast<int>(fn->GetParamCount());
	if (on == "FnColor") {
		auto* out = static_cast<unsigned char*>(obj);
		out[0] = out[1] = out[2] = out[3] = 0;
		if (n >= 4) {
			for (int i = 0; i < 4; ++i)
				out[i] = static_cast<unsigned char>(gen->GetArgDWord(i) & 0xff);
		} else if (n == 1 && (gen->GetArgTypeId(0) & asTYPEID_MASK_OBJECT)) {
			const auto* p = static_cast<const unsigned char*>(gen->GetArgAddress(0));
			if (p)
				for (int i = 0; i < 4; ++i)
					out[i] = p[i];
		}
		return true;
	}
	bool is2 = on == "FnVector2", is3 = on == "FnVector3", is4 = on == "FnVector4";
	if (!is2 && !is3 && !is4)
		return false;
	float* out = static_cast<float*>(obj);
	const int comps = is2 ? 2 : (is3 ? 3 : 4);
	const int slots = std::max(comps, static_cast<int>(ot->GetSize() / sizeof(float)));
	for (int i = 0; i < slots; ++i)
		out[i] = 0.0f;
	if (n >= 1) {
		int tid = gen->GetArgTypeId(0);
		if (tid == asTYPEID_FLOAT) {
			for (int i = 0; i < n && i < comps; ++i)
				out[i] = gen->GetArgFloat(i);
		} else {
			asITypeInfo* st = gen->GetEngine()->GetTypeInfoById(tid);
			std::string sn = st ? st->GetName() : "";
			int sc = sn == "FnVector2" ? 2 : (sn == "FnVector3" ? 3 : (sn == "FnVector4" ? 4 : comps));
			read_vec(gen, 0, out, std::min(sc, comps));
		}
	}
	return true;
}

// The FnVector* value types are plain float tuples; scripts do real arithmetic
// on them (positions come from `a + b`), so evaluate the operators instead of
// letting the stub return zeroes.
bool Registry::vec_binary(asIScriptGeneric* gen, std::string_view op) {
	asIScriptFunction* fn = gen->GetFunction();
	asITypeInfo* ot = fn ? fn->GetObjectType() : nullptr;
	if (!ot)
		return false;
	std::string on = ot->GetName();
	int comps = on == "FnVector2" ? 2 : on == "FnVector3" ? 3 : on == "FnVector4" ? 4 : 0;
	if (!comps)
		return false;
	const float* a = static_cast<const float*>(gen->GetObject());
	if (!a)
		return false;
	// op*Assign modifies `this` in place and returns it; the plain operators
	// return into caller storage.
	const bool assign = op.size() > 6 && op.substr(op.size() - 6) == "Assign";
	float* r = assign ? const_cast<float*>(a) : static_cast<float*>(gen->GetAddressOfReturnLocation());
	if (!r)
		return false;
	const bool scalar = gen->GetArgTypeId(0) == asTYPEID_FLOAT;
	const float s = scalar ? gen->GetArgFloat(0) : 0.0f;
	const float* b = scalar ? nullptr : static_cast<const float*>(gen->GetArgAddress(0));
	for (int i = 0; i < comps; ++i) {
		const float y = scalar ? s : (b ? b[i] : 0.0f);
		if (op.rfind("opAdd", 0) == 0)
			r[i] = a[i] + y;
		else if (op.rfind("opSub", 0) == 0)
			r[i] = a[i] - y;
		else if (op.rfind("opMul", 0) == 0)
			r[i] = a[i] * y;
		else if (op.rfind("opDiv", 0) == 0)
			r[i] = y != 0.0f ? a[i] / y : 0.0f;
	}
	if (assign)
		gen->SetReturnAddress(const_cast<float*>(a));
	return true;
}

void Registry::trace_event(std::string_view kind, std::string_view event, std::string_view fields) {
	if (_trace)
		_trace({frame, kind, event, fields});
}

bool Registry::dispatch(asIScriptGeneric* gen) {
	asIScriptFunction* fn = gen->GetFunction();
	if (!fn || !fn->GetName())
		return false;
	std::string_view name = fn->GetName();
	trace_event("vm", "call", "\"name\":\"" + trace::quote(name) + "\"");
	NativeArgs native_args(gen);

	if (dispatch_animation(name, native_args) != DispatchResult::Unhandled ||
		dispatch_movement(name, native_args) != DispatchResult::Unhandled ||
		dispatch_camera(name, native_args) != DispatchResult::Unhandled ||
		dispatch_text(name, native_args) != DispatchResult::Unhandled ||
		dispatch_effects(name, native_args) != DispatchResult::Unhandled ||
		dispatch_ui(name, native_args) != DispatchResult::Unhandled ||
		dispatch_debug(name, native_args) != DispatchResult::Unhandled ||
		dispatch_field(name, native_args) != DispatchResult::Unhandled ||
		dispatch_save(name, native_args) != DispatchResult::Unhandled ||
		dispatch_sprite(name, native_args) != DispatchResult::Unhandled)
		return true;

	if (name == "Wait") {
		long n = static_cast<long>(gen->GetArgDWord(0));
		pending_wait = std::max(1L, n);
		trace_event("vm", "wait", "\"frames\":" + trace::integer(n) + ",\"end\":" + trace::integer(frame + n));
		if (asIScriptContext* ctx = asGetActiveContext())
			ctx->Suspend();
		return true;
	}
	if (name == "WaitFadeDone") {
		const long end = fade_end();
		if (end > frame) {
			pending_wait = end - frame;
			spdlog::debug("wait: FadeDone frame={} end={}", frame, frame + pending_wait);
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return true;
	}
	if (name == "ObjectSetupWait" || name == "SetSkipDisable")
		return true; // modelled as immediate while the engine action has no runtime

	// Math add-on. The host has no scriptmath add-on, so the module's sin/cos/…
	// imports are registered as generic stubs (return 0); compute real values.
	if (name == "sin") {
		gen->SetReturnFloat(std::sin(gen->GetArgFloat(0)));
		return true;
	}
	if (name == "cos") {
		gen->SetReturnFloat(std::cos(gen->GetArgFloat(0)));
		return true;
	}
	if (name == "atan") {
		gen->SetReturnFloat(std::atan(gen->GetArgFloat(0)));
		return true;
	}
	if (name == "atan2") {
		gen->SetReturnFloat(std::atan2(gen->GetArgFloat(0), gen->GetArgFloat(1)));
		return true;
	}
	{
		asITypeInfo* ot = fn->GetObjectType();
		const bool vec2 = ot && std::string_view(ot->GetName()) == "FnVector2";
		if (vec2 && name == "Length") {
			const float* a = static_cast<const float*>(gen->GetObject());
			gen->SetReturnFloat(a ? std::sqrt(a[0] * a[0] + a[1] * a[1]) : 0.0f);
			return true;
		}
		if (vec2 && name == "Normalize") {
			float* a = static_cast<float*>(gen->GetObject());
			if (a) {
				const float len = std::sqrt(a[0] * a[0] + a[1] * a[1]);
				const float target = gen->GetArgFloat(0);
				if (len > 1e-6f) {
					a[0] = a[0] / len * target;
					a[1] = a[1] / len * target;
				}
				gen->SetReturnFloat(len);
			}
			return true;
		}
		if (vec2 && name == "IncDegDistance") {
			float* a = static_cast<float*>(gen->GetObject());
			if (a) {
				const float rad = gen->GetArgFloat(0) * 3.14159265358979323846f / 180.0f;
				const float dist = gen->GetArgFloat(1);
				a[0] += dist * std::cos(rad);
				a[1] += dist * std::sin(rad);
			}
			return true;
		}
	}

	if (name.size() >= 4 && name.substr(0, 4) == "$beh")
		return construct_value(gen);

	if ((name == "opAdd" || name == "opSub" || name == "opMul" || name == "opDiv" || name == "opAddAssign" ||
		 name == "opSubAssign" || name == "opMulAssign" || name == "opDivAssign") &&
		vec_binary(gen, name))
		return true;

	// --- Phase 5A: drama playback (behaviour + record only; rendering deferred).
	if (name == "IsSkipMode") {
		gen->SetReturnByte(_skip_mode ? 1 : 0);
		return true;
	}
	if (name == "CancelSkipMode") {
		_skip_mode = false;
		return true;
	}
	if (name == "CancelAllSkipMode") {
		_skip_mode = false;
		return true;
	}

	// --- 5A residuals: non-`Dr` aliases and synchronous movement/camera variants.
	if (name == "ObjectDeleteWait")
		return true;
	if (name == "SetDemoClipRange")
		return true;
	if (name == "WaitAnyButton" || name == "WaitOKButton")
		return true; // input waits cannot be satisfied headlessly

	if (dispatch_battle(name, native_args) != DispatchResult::Unhandled)
		return true;

	// All native API names are matched. The battle/AI simulation itself is
	// TODO(phase7) inside dispatch_battle (low-priority backlog in HANDOVER.md).

	// Unmodelled call. With ASB_CENSUS set, report it so a run can enumerate the
	// natives a scene depends on (returns stay at their stub defaults).
	if (std::getenv("ASB_CENSUS"))
		std::fprintf(stderr, "[census] %s%s%s\n", fn->GetNamespace(), *fn->GetNamespace() ? "::" : "", fn->GetName());

	return false;
}

void Registry::fade_at(long f, float* out) const { asb::fade_at(_timeline.fades, f, out); }

void Registry::push_fade(const float* to, long time) {
	float from[4];
	if (_timeline.fades.empty()) {
		// Before the first fade the engine's overlay is opaque when the scene is
		// about to fade in (target transparent) and clear when it fades out.
		from[0] = to[0];
		from[1] = to[1];
		from[2] = to[2];
		from[3] = to[3] < 0.5f ? 1.0f : 0.0f;
	} else {
		fade_at(frame, from);
	}
	const long t = std::max(1L, time);
	_timeline.fades.push_back(
		ScreenFade{frame, frame + t, {from[0], from[1], from[2], from[3]}, {to[0], to[1], to[2], to[3]}});
}

long Registry::fade_end() const { return _timeline.fades.empty() ? 0 : _timeline.fades.back().end; }

void Registry::set_camera_ease(CameraTween& tween, long duration) const {
	const float d = static_cast<float>(std::max(0L, duration));
	if (_timeline.camera.param_set) {
		tween.ease_in = std::clamp(_timeline.camera.accel * d, 0.0f, d);
		tween.ease_out = std::clamp(_timeline.camera.decel * d, 0.0f, d);
	} else {
		tween.ease_in = std::clamp(d * 0.1f, 0.0f, 60.0f);
		tween.ease_out = std::clamp(d * 0.9f, 0.0f, 120.0f);
	}
}

void Registry::advance_camera() {
	auto& cam = _timeline.camera;
	auto& tween = cam.tween;
	if (tween.active) {
		const long elapsed = frame - tween.start_frame;
		if (elapsed <= 0) {
			for (int k = 0; k < 3; ++k)
				cam.pos[k] = tween.start_pos[k];
		} else if (frame >= tween.end_frame) {
			for (int k = 0; k < 3; ++k)
				cam.pos[k] = tween.target_pos[k];
			tween.active = false;
			mark_dirty();
		} else {
			const float t =
				camera_tween_progress(elapsed, tween.end_frame - tween.start_frame, tween.ease_in, tween.ease_out);
			for (int k = 0; k < 3; ++k)
				cam.pos[k] = tween.start_pos[k] + (tween.target_pos[k] - tween.start_pos[k]) * t;
			mark_dirty();
		}
	}
	if (tween.zoom_active) {
		const long elapsed = frame - tween.start_frame;
		if (elapsed <= 0) {
			cam.zoom = tween.start_zoom;
		} else if (frame >= tween.end_frame) {
			cam.zoom = tween.target_zoom;
			tween.zoom_active = false;
			mark_dirty();
		} else {
			const float t =
				camera_tween_progress(elapsed, tween.end_frame - tween.start_frame, tween.ease_in, tween.ease_out);
			cam.zoom = tween.start_zoom + (tween.target_zoom - tween.start_zoom) * t;
			mark_dirty();
		}
	}
	if (_trace && (tween.active || tween.zoom_active)) {
		const long elapsed = frame - tween.start_frame;
		const long duration = std::max(0L, tween.end_frame - tween.start_frame);
		const float progress =
			duration > 0 ? camera_tween_progress(elapsed, duration, tween.ease_in, tween.ease_out) : 1.0f;
		trace_event(
			"camera",
			"eval",
			"\"pos\":" + trace::vec3(cam.pos) + ",\"zoom\":" + trace::number(cam.zoom) +
				",\"start\":" + trace::integer(tween.start_frame) + ",\"end\":" + trace::integer(tween.end_frame) +
				",\"progress\":" + trace::number(progress) + ",\"ease_in\":" + trace::number(tween.ease_in) +
				",\"ease_out\":" + trace::number(tween.ease_out));
	}
}

void Registry::advance() {
	_timeline.frame = frame;
	for (auto it = _moves.begin(); it != _moves.end();) {
		auto bi = _by_name.find(it->first);
		if (bi == _by_name.end()) {
			it = _moves.erase(it);
			continue;
		}
		Move& m = it->second;
		const float t = m.f1 > m.f0
							? std::clamp(static_cast<float>(frame - m.f0) / static_cast<float>(m.f1 - m.f0), 0.0f, 1.0f)
							: 1.0f;
		float* p = _timeline.entities[bi->second].pos;
		for (int k = 0; k < 3; ++k)
			p[k] = m.from[k] + (m.to[k] - m.from[k]) * t;
		if (m.arc != 0.0f)
			p[2] += m.arc * 4.0f * t * (1.0f - t);
		mark_dirty();
		if (frame >= m.f1) {
			auto& entity = _timeline.entities[bi->second];
			_animations.finish_motion(entity, m.animation_token, m.followup_track, m.followup_track_index, frame);
			it = _moves.erase(it);
		} else
			++it;
	}
	for (auto& entity : _timeline.entities) {
		if (_animations.advance(entity, frame))
			mark_dirty();
	}
	advance_camera();
}

void Registry::flush_keyframe() {
	advance();
	if (!_dirty && !_timeline.keyframes.empty())
		return;
	Keyframe k;
	k.frame = frame;
	k.entities = _timeline.entities;
	k.camera = _timeline.camera;
	_timeline.keyframes.push_back(std::move(k));
	_dirty = false;
}

} // namespace asb
