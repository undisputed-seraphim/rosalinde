#include "registry.hpp"

#include <angelscript.h>
#include <scriptarray/scriptarray.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string_view>

namespace asb {

namespace {

bool wait_trace() {
	static const bool on = std::getenv("SCENE_TRACE") != nullptr;
	return on;
}

std::string read_string(asIScriptGeneric* gen, int i) {
	auto* p = static_cast<const std::string*>(gen->GetArgAddress(i));
	return p ? *p : std::string();
}

void read_vec(asIScriptGeneric* gen, int i, float* out, int n) {
	auto* p = static_cast<const float*>(gen->GetArgAddress(i));
	for (int k = 0; k < n; ++k)
		out[k] = p ? p[k] : 0.0f;
}

// FnColor is a packed 4-byte RGBA value type.
void read_color(asIScriptGeneric* gen, int i, float* out) {
	auto* p = static_cast<const unsigned char*>(gen->GetArgAddress(i));
	for (int k = 0; k < 4; ++k)
		out[k] = (p ? p[k] : 0.0f) / 255.0f;
}

bool plausible_scale(float s) { return std::isfinite(s) && s > 1e-4f && s < 1e4f; }

// Opt-in trace (SCENE_TRACE) of the scene-relevant natives as the module runs.
bool scene_trace() {
	static const bool on = std::getenv("SCENE_TRACE") != nullptr;
	return on;
}

// Opt-in trace (ASB_TEXT) of the dialogue/balloon messages a script emits.
bool text_trace() {
	static const bool on = std::getenv("ASB_TEXT") != nullptr || std::getenv("SCENE_TRACE") != nullptr;
	return on;
}

std::vector<int> read_int_array(asIScriptGeneric* gen, int i) {
	std::vector<int> out;
	auto* arr = static_cast<CScriptArray*>(gen->GetArgObject(i));
	if (!arr)
		return out;
	const asUINT n = std::min<asUINT>(arr->GetSize(), 64);
	out.reserve(n);
	for (asUINT k = 0; k < n; ++k)
		out.push_back(*static_cast<const int*>(arr->At(k)));
	return out;
}

// Estimated on-screen time of a message in frames (60 fps): a base plus roughly
// one frame per UTF-8 code point, so a scene's lines advance visibly.
long text_duration(const std::string& s) {
	size_t chars = 0;
	for (unsigned char c : s)
		if ((c & 0xC0) != 0x80)
			++chars;
	return 30 + static_cast<long>(chars);
}

// DrSetLayerMask / DrSetVariationType take a hexadecimal bitmask string.
unsigned parse_hex(const std::string& s) {
	unsigned v = 0;
	size_t i = (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? 2 : 0;
	for (; i < s.size(); ++i) {
		const char c = s[i];
		int d = (c >= '0' && c <= '9') ? c - '0'
			  : (c >= 'a' && c <= 'f') ? c - 'a' + 10
			  : (c >= 'A' && c <= 'F') ? c - 'A' + 10
									   : -1;
		if (d < 0)
			break;
		v = (v << 4) | static_cast<unsigned>(d);
	}
	return v;
}

} // namespace

void Registry::reset(std::string source, std::string function) {
	_timeline = Timeline{};
	_timeline.source = std::move(source);
	_timeline.function = std::move(function);
	_entities.clear();
	_by_name.clear();
	_party.clear();
	_slots.clear();
	_class_changed.clear();
	_disabled.clear();
	_quests.clear();
	_unit_dir.clear();
	_units.clear();
	_moves.clear();
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
	_shake_seq = 0;
	_follow_offset[0] = _follow_offset[1] = 0.0f;
	_balcony_bru = false;
	_balcony_scr = false;
	_camera = RuntimeCamera{};
	_dirty = false;
	frame = 0;
	pending_wait = -1;
}

int Registry::ensure(const std::string& name) {
	auto it = _by_name.find(name);
	if (it != _by_name.end())
		return it->second;
	int idx = static_cast<int>(_entities.size());
	RuntimeEntity e;
	e.name = name;
	e.order = idx;
	_entities.push_back(std::move(e));
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
	if (scene_trace())
		std::fprintf(stderr, "[vm f=%ld] create %-22s \"%s\" mbs=%s\n", frame, kind.c_str(), id.c_str(), mbs.c_str());
	int idx = ensure(id);
	auto& e = _entities[idx];
	e.kind = kind;
	e.space = RuntimeEntity::Space::World;
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
	_dirty = true;
}

void Registry::create_chara(const std::string& kind, const std::vector<std::string>& names) {
	std::string id = names.size() >= 2 ? names[1] : (names.empty() ? std::string() : names[0]);
	if (id.empty() && !names.empty())
		id = names[0];
	if (scene_trace()) {
		std::fprintf(stderr, "[vm f=%ld] chara  %-22s \"%s\"", frame, kind.c_str(), id.c_str());
		for (const auto& n : names)
			std::fprintf(stderr, " [%s]", n.c_str());
		std::fprintf(stderr, "\n");
	}
	int idx = ensure(id);
	auto& e = _entities[idx];
	e.kind = kind;
	e.space = kind == "DrUiCreateChara" ? RuntimeEntity::Space::Ui : RuntimeEntity::Space::World;
	// UI chara are created hidden and faded in later by DrUiSetColorA; world
	// chara (DrCreateChara/Npc) are visible immediately.
	if (e.space == RuntimeEntity::Space::Ui)
		e.alpha = 0;
	e.aliases = names;
	// World characters carry no asset in the call: the engine resolves the chara
	// def name through its CharaSet appearance table (the Japanese label). Try
	// that before the hand-maintained display-name table.
	if (_chara_sets && kind != "DrUiCreateChara") {
		for (const auto& n : names) {
			if (n.empty())
				continue;
			if (const CharaInfo* ci = _chara_sets->resolve(n)) {
				e.mbs = ci->mbs;
				e.ftx = ci->ftx;
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
		}
	}
	for (const auto& n : names)
		alias(idx, n);
	_dirty = true;
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
	for (int i = 0; i < comps; ++i)
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

bool Registry::dispatch(asIScriptGeneric* gen) {
	asIScriptFunction* fn = gen->GetFunction();
	if (!fn || !fn->GetName())
		return false;
	std::string_view name = fn->GetName();

	if (name == "Wait") {
		long n = static_cast<long>(gen->GetArgDWord(0));
		pending_wait = std::max(1L, n);
		if (wait_trace())
			std::fprintf(stderr, "[wait f=%ld] Wait %ld -> end %ld\n", frame, pending_wait, frame + pending_wait);
		if (asIScriptContext* ctx = asGetActiveContext())
			ctx->Suspend();
		return true;
	}
	if (name == "WaitFadeDone") {
		const long end = fade_end();
		if (end > frame) {
			pending_wait = end - frame;
			if (wait_trace())
				std::fprintf(stderr, "[wait f=%ld] FadeDone -> end %ld\n", frame, frame + pending_wait);
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return true;
	}
	if (name == "DrActionWait" || name == "ObjectSetupWait" || name == "SetSkipDisable")
		return true; // modelled as immediate while the engine action has no runtime

	// Scene queries: answer from the recorded scene so branches/computed args
	// see real values instead of the stub's zero.
	if (name == "IsObjectExist" || name == "IsExistSprite") {
		auto it = _by_name.find(read_string(gen, 0));
		const bool exists = it != _by_name.end() && _entities[it->second].visible;
		gen->SetReturnByte(exists ? 1 : 0);
		return true;
	}
	if (name == "GetObjectNameByCharaDefName") {
		// Reverse lookup over the live chara objects: find the one whose chara def
		// name matches and return its object name; "NONE" when absent (as the
		// engine does).
		const std::string dn = read_string(gen, 0);
		std::string res = "NONE";
		for (const auto& e : _entities) {
			if (!e.visible)
				continue;
			const bool is_chara =
				e.kind == "DrCreateChara" || e.kind == "DrCreateNpc" || e.kind == "DrUiCreateChara";
			bool match = e.name == dn;
			for (const auto& a : e.aliases)
				match = match || a == dn;
			if (is_chara && match) {
				res = e.aliases.empty() ? e.name : e.aliases.front();
				break;
			}
		}
		// Returning true skips the stub's default return construction, so the
		// value-type return slot must be constructed here (not assigned to).
		if (auto* out = static_cast<std::string*>(gen->GetAddressOfReturnLocation()))
			new (out) std::string(std::move(res));
		return true;
	}

	// Script-accumulated save/party state: the mutators populate the stores; the
	// getters read them back.
	if (name == "AddCharaSaveData") {
		_party.insert(read_string(gen, 0));
		return true;
	}
	if (name == "DeleteCharaSaveData" || name == "DeleteMobCharaSaveData") {
		_party.erase(read_string(gen, 0));
		return true;
	}
	if (name == "IsExistCharaSaveData") {
		gen->SetReturnByte(_party.count(read_string(gen, 0)) ? 1 : 0);
		return true;
	}
	if (name == "SetSlotFlag") {
		_slots[static_cast<int>(gen->GetArgDWord(0))] = static_cast<int>(gen->GetArgDWord(1));
		return true;
	}
	if (name == "GetSlotFlag") {
		auto it = _slots.find(static_cast<int>(gen->GetArgDWord(0)));
		gen->SetReturnDWord(it == _slots.end() ? 0u : static_cast<asDWORD>(it->second));
		return true;
	}
	if (name == "GetSlotBitFlag") {
		auto it = _slots.find(static_cast<int>(gen->GetArgDWord(0)));
		gen->SetReturnByte((it != _slots.end() && it->second) ? 1 : 0);
		return true;
	}

	if (name == "SetSlotBitFlag") {
		_slots[static_cast<int>(gen->GetArgDWord(0))] = gen->GetArgDWord(1) ? 1 : 0;
		return true;
	}
	if (name == "GetStageBitFlag") {
		auto it = _slots.find(static_cast<int>(gen->GetArgDWord(0)));
		gen->SetReturnByte((it != _slots.end() && it->second) ? 1 : 0);
		return true;
	}
	if (name == "ClassChangeChara") {
		_class_changed.insert(read_string(gen, 0));
		return true;
	}
	if (name == "IsClassChangeChara") {
		gen->SetReturnByte(_class_changed.count(read_string(gen, 0)) ? 1 : 0);
		return true;
	}
	if (name == "SetDisableChara") {
		const std::string n = read_string(gen, 0);
		if (gen->GetArgDWord(1))
			_disabled.insert(n);
		else
			_disabled.erase(n);
		return true;
	}
	if (name == "IsDisableChara") {
		gen->SetReturnByte(_disabled.count(read_string(gen, 0)) ? 1 : 0);
		return true;
	}
	if (name == "AddUnitSaveData") {
		_units.insert(static_cast<int>(gen->GetArgDWord(0)));
		return true;
	}
	{
		const bool complete = name.find("Complete") != std::string_view::npos;
		if (name == "QuestOpen" || name == "QuestComplete") {
			_quests[static_cast<int>(gen->GetArgDWord(0))] |= complete ? 3 : 1;
			return true;
		}
		if (name == "IsQuestOpened" || name == "IsQuestCompleted") {
			auto it = _quests.find(static_cast<int>(gen->GetArgDWord(0)));
			const int bit = complete ? 2 : 1;
			gen->SetReturnByte((it != _quests.end() && (it->second & bit)) ? 1 : 0);
			return true;
		}
	}
	if (name == "SetUnitDir") {
		_unit_dir[read_string(gen, 0)] = static_cast<int>(gen->GetArgDWord(1));
		return true;
	}
	if (name == "GetUnitDegree") {
		auto it = _unit_dir.find(read_string(gen, 0));
		gen->SetReturnDWord(it == _unit_dir.end() ? 0u : static_cast<asDWORD>(it->second));
		return true;
	}
	if (name == "CameraMoveWait")
		return true; // immediate: the recorded camera already reflects the move
	if (name == "ClearCamera" || name == "DrClearCamera") {
		_camera = RuntimeCamera{};
		_dirty = true;
		return true;
	}
	// Save/party getters with no script-side mutator. There is no save file and
	// none is required: the script-facing state API is entirely in-memory (owned
	// by this shim), so the plan is to invent our own save format and seed these
	// from it. TODO(save): define that format and wire it up; until then, report
	// defaults.
	if (name == "GetCharaLv" || name == "GetCharaAverageLv" || name == "GetArmyType" || name == "GetGold" ||
		name == "GetItemNum" || name == "GetFameRank") {
		gen->SetReturnDWord(0);
		return true;
	}
	if (name == "IsLeader" || name == "StIsPartnerSelectCharaDefName" || name == "IsEmptyPlayerCharaList") {
		gen->SetReturnByte((name == "IsEmptyPlayerCharaList" && _party.empty()) ? 1 : 0);
		return true;
	}

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

	if (name == "GetCameraPosV") {
		float* r = static_cast<float*>(gen->GetAddressOfReturnLocation());
		if (r) {
			r[0] = _camera.pos[0];
			r[1] = _camera.pos[1];
		}
		return true;
	}

	if (name == "GetPosV") {
		float* r = static_cast<float*>(gen->GetAddressOfReturnLocation());
		if (r) {
			r[0] = 0.0f;
			r[1] = 0.0f;
			auto it = _by_name.find(read_string(gen, 0));
			if (it != _by_name.end()) {
				r[0] = _entities[it->second].pos[0];
				r[1] = _entities[it->second].pos[1];
			}
		}
		return true;
	}

	if (name.size() >= 4 && name.substr(0, 4) == "$beh")
		return construct_value(gen);

	if ((name == "opAdd" || name == "opSub" || name == "opMul" || name == "opDiv" || name == "opAddAssign" ||
			name == "opSubAssign" || name == "opMulAssign" || name == "opDivAssign") &&
		vec_binary(gen, name))
		return true;

	auto S = [&](int i) { return read_string(gen, i); };
	auto F = [&](int i) { return gen->GetArgFloat(i); };
	auto I = [&](int i) { return static_cast<int>(gen->GetArgDWord(i)); };
	auto V = [&](int i, float* o, int n) { read_vec(gen, i, o, n); };
	auto ent = [&](int i) -> RuntimeEntity* {
		auto it = _by_name.find(read_string(gen, i));
		return it == _by_name.end() ? nullptr : &_entities[it->second];
	};

	// --- Dialogue / text (Phase 4a). Scripts pass the message text literally,
	// so record it (with speaker, balloon style and position) and model the
	// UcMsgBalloonParam / UcSpriteTextParam objects the setters mutate. Pixel
	// rendering is deferred (4b); `ASB_TEXT`/`SCENE_TRACE` logs the text.
	asITypeInfo* ot = fn->GetObjectType();
	const std::string otn = (ot && ot->GetName()) ? ot->GetName() : "";
	void* self = gen->GetObject();

	auto balloon_of = [&](int arg) -> BalloonParam* {
		void* p = gen->GetArgObject(arg);
		if (!p)
			p = gen->GetArgAddress(arg);
		auto it = _balloons.find(p);
		return it == _balloons.end() ? nullptr : &it->second;
	};

	auto emit = [&](RuntimeText::Kind kind, std::string speaker, std::string text, int ids_arg, int balloon_arg, int pos_arg) -> RuntimeText& {
		RuntimeText t;
		t.frame = frame;
		t.kind = kind;
		t.speaker = std::move(speaker);
		t.text = std::move(text);
		if (ids_arg >= 0) {
			// The Msg ID array is engine-internal (voice ids); the large ones are
			// uninitialised in our model, so only record compact arrays.
			if (auto* a = static_cast<CScriptArray*>(gen->GetArgObject(ids_arg)))
				if (a->GetSize() <= 64)
					t.ids = read_int_array(gen, ids_arg);
		}
		if (balloon_arg >= 0) {
			if (BalloonParam* b = balloon_of(balloon_arg)) {
				t.disp_type = b->disp_type;
				t.disp_name = b->disp_name;
				t.face_dir_x = b->face_dir_x;
				t.flags = b->flags;
			}
		}
		if (pos_arg >= 0)
			V(pos_arg, t.pos, 2);
		t.end = frame + text_duration(t.text);
		if (text_trace()) {
			std::fprintf(
				stderr,
				"[text f=%ld] kind=%d speaker=\"%s\" disp=%d name=%d dir=%d flags=%x pos=(%.1f,%.1f): %s\n",
				frame,
				static_cast<int>(kind),
				t.speaker.c_str(),
				t.disp_type,
				t.disp_name,
				t.face_dir_x,
				t.flags,
				t.pos[0],
				t.pos[1],
				t.text.c_str());
		}
		_timeline.texts.push_back(std::move(t));
		return _timeline.texts.back();
	};

	// Msg/Dialog/Select block until the message has been read; park the script
	// for the estimated display time and let the scheduler resume it.
	auto block_text = [&]() -> bool {
		if (_timeline.texts.empty())
			return true;
		const long end = _timeline.texts.back().end;
		const long d = std::max(1L, end - frame);
		pending_wait = d;
		if (wait_trace())
			std::fprintf(stderr, "[wait f=%ld] text -> end %ld\n", frame, end);
		if (asIScriptContext* ctx = asGetActiveContext())
			ctx->Suspend();
		return true;
	};

	if (otn == "UcMsgBalloonParam") {
		if (name == "SetDispType")
			_balloons[self].disp_type = I(0);
		else if (name == "SetDispName")
			_balloons[self].disp_name = I(0);
		else if (name == "SetFaceDirX")
			_balloons[self].face_dir_x = I(0);
		else if (name == "AddBalloonFlag")
			_balloons[self].flags |= static_cast<unsigned>(I(0));
		else if (name == "DelBalloonFlag")
			_balloons[self].flags &= ~static_cast<unsigned>(I(0));
		else if (name == "opAssign") {
			if (BalloonParam* src = balloon_of(0))
				_balloons[self] = *src;
			gen->SetReturnAddress(self);
		}
		return true;
	}
	if (otn == "UcSpriteTextParam") {
		SpriteTextParam& p = _sprite_texts[self];
		if (name == "SetAlign")
			p.align = I(0);
		else if (name == "SetFontType")
			p.font_type = I(0);
		else if (name == "SetOutline")
			p.outline = I(0);
		else if (name == "SetColorR")
			p.color[0] = I(0);
		else if (name == "SetColorG")
			p.color[1] = I(0);
		else if (name == "SetColorB")
			p.color[2] = I(0);
		else if (name == "SetScale")
			p.scale = F(0);
		else if (name == "SetBaseLine")
			p.base_line = F(0);
		else if (name == "SetForceInstant")
			p.force_instant = I(0) != 0;
		else if (name == "opAssign") {
			void* src = gen->GetArgObject(0);
			if (!src)
				src = gen->GetArgAddress(0);
			auto it = _sprite_texts.find(src);
			if (it != _sprite_texts.end())
				p = it->second;
			gen->SetReturnAddress(self);
		}
		return true;
	}

	if (name == "Msg" || name == "MsgScreen") {
		const bool screen = name == "MsgScreen";
		emit(screen ? RuntimeText::Kind::Screen : RuntimeText::Kind::Msg, S(0), S(1), 2, 3, 4);
		return block_text();
	}
	if (name == "MsgAsync" || name == "MsgScreenAsync") {
		emit(name == "MsgScreenAsync" ? RuntimeText::Kind::Screen : RuntimeText::Kind::Msg, S(0), S(1), 2, 3, 4);
		return true;
	}
	if (name == "SelectMsg" || name == "SelectMsgScreen") {
		emit(RuntimeText::Kind::Select, S(0), S(1), 2, 3, 4);
		_select_result = 0; // deterministic default: first option
		return block_text();
	}
	if (name == "MsgStageVoice") {
		emit(RuntimeText::Kind::Msg, S(0), {}, -1, 2, 3);
		return block_text();
	}
	if (name == "Dialog" || name == "YNDialog") {
		emit(name == "YNDialog" ? RuntimeText::Kind::YesNo : RuntimeText::Kind::Dialog, {}, S(0), -1, -1, -1);
		if (name == "YNDialog")
			_select_result = 0;
		return block_text();
	}
	if (name == "CommonMsg") {
		emit(RuntimeText::Kind::Msg, {}, S(0), -1, -1, -1);
		return block_text();
	}
	if (name == "StShowTutorialMsg") {
		emit(RuntimeText::Kind::Msg, {}, S(1), -1, -1, -1);
		return true;
	}
	if (name == "Message") {
		RuntimeText& t = emit(RuntimeText::Kind::Message, {}, S(0), -1, -1, -1);
		t.end = 0; // banner is open until CloseMessage()
		if (I(1) >= 0)
			t.ids.push_back(I(1));
		return true;
	}
	if (name == "CloseMessage") {
		for (auto it = _timeline.texts.rbegin(); it != _timeline.texts.rend(); ++it) {
			if (it->kind == RuntimeText::Kind::Message && it->end == 0) {
				it->end = frame;
				break;
			}
		}
		return true;
	}
	if (name == "MsgFinishWait") {
		// Async messages finish here: park until the last one's display ends.
		if (!_timeline.texts.empty() && _timeline.texts.back().end > frame) {
			pending_wait = _timeline.texts.back().end - frame;
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return true;
	}
	if (name == "CloseMsg") {
		if (!_timeline.texts.empty())
			_timeline.texts.back().end = frame;
		return true;
	}
	if (name == "StCloseTutorialMsg" || name == "DrAlwaysUpdateMsgPos")
		return true;
	if (name == "GetResultSelectMsg" || name == "GetYNDialogResult") {
		gen->SetReturnDWord(static_cast<asDWORD>(_select_result));
		return true;
	}
	if (name == "CreateSpriteText") {
		emit(RuntimeText::Kind::Msg, S(0), S(1), -1, -1, 3);
		gen->SetReturnDWord(static_cast<asDWORD>(_sprite_text_handle++));
		return true;
	}
	if (name == "SpriteTextFinishWait" || name == "MulSpriteTextOffsetY")
		return true;
	if (name == "SetMsgFaceAnimation" || name == "SetMsgFaceLayerMask" || name == "ShowAddCharaDialog")
		return true;

	// Movement tweens: Move (relative delta) / MoveAbs (absolute) + the drama
	// DrMove* family set a position tween; MoveWait/MoveWaitAll park the script
	// until it finishes. Easing (the trailing int) is ignored.
	if (name == "Move" || name == "MoveAbs") {
		float to[2] = {0.0f, 0.0f};
		V(1, to, 2);
		if (RuntimeEntity* e = ent(0)) {
			Move m{};
			for (int k = 0; k < 2; ++k) {
				m.from[k] = e->pos[k];
				m.to[k] = (name == "Move") ? e->pos[k] + to[k] : to[k];
			}
			m.from[2] = m.to[2] = e->pos[2];
			m.f0 = frame;
			m.f1 = frame + std::max(1, I(2));
			_moves[e->name] = m;
			_dirty = true;
		}
		return true;
	}
	if (name == "DrMoveAbs" || name == "DrMoveAbsAsync" || name == "DrMoveAsync" || name == "DrShortMoveAsync" ||
		name == "DrMove") {
		float to[3] = {0.0f, 0.0f, 0.0f};
		V(1, to, 3);
		if (RuntimeEntity* e = ent(0)) {
			Move m{};
			for (int k = 0; k < 3; ++k) {
				m.from[k] = e->pos[k];
				m.to[k] = to[k];
			}
			int dur = I(4);
			if (dur <= 0)
				dur = 30;
			m.f0 = frame;
			m.f1 = frame + dur;
			_moves[e->name] = m;
			_dirty = true;
			if (name == "DrMove") {
				pending_wait = dur;
				if (asIScriptContext* ctx = asGetActiveContext())
					ctx->Suspend();
			}
		}
		return true;
	}
	if (name == "MoveWait" || name == "MoveWaitAll") {
		const std::string only = name == "MoveWait" ? S(0) : std::string();
		long wait = 0;
		for (const auto& [n, m] : _moves) {
			if (!only.empty() && n != only)
				continue;
			wait = std::max(wait, m.f1 - frame);
		}
		if (wait > 0) {
			pending_wait = wait;
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return true;
	}
	if (name == "FollowDir" || name == "DrTurnAsync") {
		if (RuntimeEntity* e = ent(0)) {
			auto it = _by_name.find(S(1));
			if (it != _by_name.end()) {
				e->dir_x = _entities[it->second].pos[0] < e->pos[0] ? -1.0f : 1.0f;
				_dirty = true;
			}
		}
		return true;
	}
	// Scene reads.
	if (name == "DrGetPosV" || name == "DrGetCollisionPosV") {
		float* r = static_cast<float*>(gen->GetAddressOfReturnLocation());
		if (r) {
			r[0] = r[1] = r[2] = 0.0f;
			if (RuntimeEntity* e = ent(0))
				for (int k = 0; k < 3; ++k)
					r[k] = e->pos[k];
		}
		return true;
	}
	if (name == "DrGetDirX") {
		int d = 1;
		if (RuntimeEntity* e = ent(0))
			d = e->dir_x < 0 ? -1 : 1;
		gen->SetReturnDWord(static_cast<asDWORD>(d));
		return true;
	}
	// State stores.
	if (name == "DbSetDebugParam") {
		_db_params[S(0)] = I(1);
		return true;
	}
	if (name == "DbGetDebugParam") {
		auto it = _db_params.find(S(0));
		gen->SetReturnDWord(it == _db_params.end() ? 0u : static_cast<asDWORD>(it->second));
		return true;
	}
	if (name == "DbGetLocalParam") {
		auto it = _db_params.find(S(0));
		gen->SetReturnFloat(it == _db_params.end() ? 0.0f : static_cast<float>(it->second));
		return true;
	}
	if (name == "AddToHelpList") {
		_help.insert(S(0));
		return true;
	}
	if (name == "ShowTips")
		return true; // UI tip hint: no runtime model

	// Wait until a sprite's animation finishes. Only modelled when a length
	// provider knows the track; otherwise it stays immediate.
	if (name == "WaitSpriteMoneAnimationEnd" || name == "DrAnimationWait") {
		long n = 0;
		RuntimeEntity* e = ent(0);
		if (e && _track_frames)
			n = _track_frames(e->mbs, e->track_index);
		if (wait_trace())
			std::fprintf(
				stderr,
				"[wait f=%ld] AnimEnd \"%s\" track=%d mbs=%s -> +%ld (end %ld)\n",
				frame,
				e ? e->name.c_str() : "?",
				e ? e->track_index : -1,
				e ? e->mbs.c_str() : "",
				n,
				frame + n);
		if (n > 0) {
			pending_wait = n;
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return true;
	}

	// CreateSpriteMone(name, mbs, ftx, track, FnVector2 pos, layer, scale, bool,
	// int, int, int): the first three string args are the id, the .mbs and the
	// .ftx (pushed in reverse, so S(0) is the name the mutators key on).
	if (name == "CreateSpriteMone" || name == "CreateSpriteEffect" || name == "CreateSpriteMoneWorld") {
		float p[3] = {0.0f, 0.0f, 0.0f};
		V(4, p, 2);
		create_sprite(std::string(name), S(0), S(1), S(2), p, I(5), F(6));
		return true;
	}
	if (name == "CreateSpriteMoneStageStart") {
		float p[3] = {0.0f, 0.0f, 0.0f};
		V(1, p, 2);
		create_sprite(std::string(name), S(0), "", "", p, I(2), F(3));
		return true;
	}
	if (name == "CreateSpriteCoA") {
		float p[3] = {0.0f, 0.0f, 0.0f};
		V(2, p, 2);
		create_sprite(std::string(name), S(0), "", "", p, I(3), F(4));
		return true;
	}

	// Stage/weather/flag configuration. No id->effect mapping is known, so the
	// visual is left to the engine; record the ids for offline inspection.
	if (name == "DrCreateStage") {
		StageConfig s;
		s.frame = frame;
		s.stage_type = I(0);
		s.weather_type = I(1);
		s.coa_preset = I(2);
		s.flag_color = I(3);
		_timeline.stages.push_back(std::move(s));
		return true;
	}

	if (name == "DrUiCreateChara") {
		create_chara(std::string(name), {S(0), S(1), S(2), S(3)});
		return true;
	}
	if (name == "DrCreateChara" || name == "DrCreateNpc") {
		create_chara(std::string(name), {S(0), S(1)});
		return true;
	}

	if (name == "DrSetPosV" || name == "SetSpritePosV" || name == "SetPosV" || name == "DrUiSetPosV" ||
		name == "DrSetOffsetPosV" || name == "DrSetEffectPosV" || name == "DrSetEffectOffsetV") {
		if (RuntimeEntity* e = ent(0)) {
			float p[3] = {0.0f, 0.0f, 0.0f};
			V(1, p, 3);
			e->pos[0] = p[0];
			e->pos[1] = p[1];
			e->pos[2] = p[2];
			_dirty = true;
		}
		return true;
	}

	// Anchor one sprite to another: the child's position becomes relative to
	// the target's (both are usually at the origin, but moving parents matter).
	if (name == "SetSpriteToAnchor") {
		if (RuntimeEntity* e = ent(0)) {
			e->parent = S(1);
			_dirty = true;
		}
		return true;
	}

	if (name == "SetSpriteScale" || name == "DrSetScale" || name == "SetSpriteMoneScale" || name == "DrUiSetScale") {
		if (RuntimeEntity* e = ent(0)) {
			float s = F(1);
			if (plausible_scale(s)) {
				e->scale = s;
				_dirty = true;
			}
		}
		return true;
	}

	// Playback direction / SE / pre-render hints. The renderer has no reverse or
	// per-frame list model yet, so record the direction and accept the rest.
	if (name == "SetSpriteMoneReverse") {
		if (RuntimeEntity* e = ent(0)) {
			e->reverse = I(1) != 0;
			_dirty = true;
		}
		return true;
	}
	if (name == "SetSpriteMoneStopSE" || name == "SetSpriteMoneDispOffFrameList" || name == "SetSpriteMonePreRenderAll")
		return true;

	if (name == "GetSpriteMoneFindCallbackTiming") {
		gen->SetReturnDWord(0);
		return true;
	}

	if (name == "SetSpriteMoneAnimation" || name == "SetSpriteMoneAnimationPaused") {
		if (RuntimeEntity* e = ent(0)) {
			e->track_index = I(1);
			if (scene_trace())
				std::fprintf(stderr, "[vm f=%ld] anim   \"%s\" track=%d\n", frame, e->name.c_str(), e->track_index);
			_dirty = true;
		}
		return true;
	}
	if (name == "DrUiSetAnimation") {
		if (RuntimeEntity* e = ent(0)) {
			e->track_index = I(1);
			if (scene_trace())
				std::fprintf(stderr, "[vm f=%ld] anim   \"%s\" track=%d\n", frame, e->name.c_str(), e->track_index);
			_dirty = true;
		}
		return true;
	}
	if (name == "DrSetIdleAnimation" || name == "DrAnimation" || name == "DrAnimationAsync" ||
		name == "DrChangeAnimation") {
		if (RuntimeEntity* e = ent(0)) {
			e->track = S(1);
			if (!e->track.empty() && _resolve_track) {
				int ti = _resolve_track(e->mbs, e->track);
				if (ti >= 0)
					e->track_index = ti;
			}
			if (scene_trace())
				std::fprintf(
					stderr,
					"[vm f=%ld] anim   \"%s\" track=\"%s\" idx=%d\n",
					frame,
					e->name.c_str(),
					e->track.c_str(),
					e->track_index);
			_dirty = true;
		}
		return true;
	}

	if (name == "DrUiSetDirX" || name == "SetSpriteDirX" || name == "DrSetDirX") {
		if (RuntimeEntity* e = ent(0)) {
			e->dir_x = I(1) < 0 ? -1.0f : 1.0f;
			_dirty = true;
		}
		return true;
	}

	// DrUiSetColorA(name, value, time): the value is the target grey/alpha (0 at
	// creation, 255 when fully faded in); fade time is ignored. Track the low
	// byte so a not-yet-faded-in sprite stays hidden.
	if (name == "DrUiSetColorA" || name == "SetSpriteColorA" || name == "DrSetColorA" || name == "SetColorA") {
		if (RuntimeEntity* e = ent(0)) {
			e->alpha = I(1) & 0xff;
			_dirty = true;
		}
		return true;
	}

	if (name == "DeleteSprite" || name == "DrUiDeleteChara" || name == "DrDeleteEffect" || name == "DeleteEffect" ||
		name == "DeleteObject") {
		if (RuntimeEntity* e = ent(0)) {
			if (scene_trace())
				std::fprintf(
					stderr, "[vm f=%ld] delete %-22s \"%s\"\n", frame, std::string(name).c_str(), e->name.c_str());
			e->visible = false;
			_dirty = true;
		}
		return true;
	}

	if (name == "DrGetCameraPosV") {
		float* r = static_cast<float*>(gen->GetAddressOfReturnLocation());
		if (r)
			for (int k = 0; k < 3; ++k)
				r[k] = _camera.pos[k];
		return true;
	}

	if (name == "DrSetCameraPosV" || name == "SetCameraPosV" || name == "SetCameraTargetPosV" ||
		name == "SetCameraTargetPosVBySpeed") {
		float p[3] = {0.0f, 0.0f, 0.0f};
		V(0, p, 3);
		_camera.pos[0] = p[0];
		_camera.pos[1] = p[1];
		_camera.pos[2] = p[2];
		_camera.has_pos = true;
		if (name == "DrSetCameraPosV")
			_camera_move_end = frame + std::max(0, I(1));
		_dirty = true;
		return true;
	}
	if (name == "DrSetCameraZoom") {
		float z = F(0);
		if (plausible_scale(z)) {
			_camera.zoom = z;
			_camera.has_zoom = true;
			_dirty = true;
		}
		return true;
	}
	if (name == "DrSetCameraParam" || name == "SetCameraParam") {
		_camera.accel = F(0);
		_camera.decel = F(1);
		_dirty = true;
		return true;
	}

	// Whole-screen colour fades: ColorFadeAsync(FnColor, time) ramps the
	// overlay to `color`; CrossFadeAsync(time) is modelled as a dip to black.
	if (name == "ColorFadeAsync" || name == "ColorFade2" || name == "ColorFade" || name == "ColorFade2Async") {
		float c[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		read_color(gen, 0, c);
		push_fade(c, I(1));
		return true;
	}
	if (name == "CrossFadeAsync") {
		const long t = std::max(1L, static_cast<long>(I(0)));
		const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		float from[4];
		if (_timeline.fades.empty()) {
			from[0] = from[1] = from[2] = 0.0f;
			from[3] = 0.0f;
		} else {
			fade_at(frame, from);
		}
		_timeline.fades.push_back(ScreenFade{
			frame, frame + t, {from[0], from[1], from[2], from[3]}, {black[0], black[1], black[2], black[3]}});
		float mid[4];
		fade_at(frame + t, mid);
		_timeline.fades.push_back(ScreenFade{
			frame + t, frame + 2 * t, {mid[0], mid[1], mid[2], mid[3]}, {clear[0], clear[1], clear[2], clear[3]}});
		return true;
	}

	// --- Phase 5A: drama playback (behaviour + record only; rendering deferred).
	if (name == "IsSkipMode") {
		gen->SetReturnByte(_skip_mode ? 1 : 0);
		return true;
	}
	if (name == "CancelSkipMode") {
		_skip_mode = false;
		return true;
	}
	if (name == "DbIsDebugDrama" || name == "DrIsArchiveMode" || name == "FlIsNightTime") {
		gen->SetReturnByte(0);
		return true;
	}
	if (name == "DrSetLayerMask") {
		const unsigned mask = parse_hex(S(1));
		// Verified from DrLib::DrFacial/DrClearFacial bytecode: the flag is
		// `!show` — true clears (hides) the masked layers, false shows them.
		if (RuntimeEntity* e = ent(0)) {
			if (I(2))
				e->layer_mask &= ~mask;
			else
				e->layer_mask |= mask;
			_dirty = true;
		}
		return true;
	}
	if (name == "DrSetVariationType") {
		const unsigned mask = parse_hex(S(0));
		if (I(1))
			_variation |= mask;
		else
			_variation &= ~mask;
		_timeline.variations.push_back(VariationEvent{frame, _variation});
		_dirty = true;
		return true;
	}
	if (name == "DrSetBreathOff") {
		if (RuntimeEntity* e = ent(0)) {
			e->breath_off = I(1) != 0;
			_dirty = true;
		}
		return true;
	}
	if (name == "DrSetLipSync") {
		if (RuntimeEntity* e = ent(0)) {
			e->lipsync = I(1) != 0;
			_dirty = true;
		}
		return true;
	}
	if (name == "DrTurn") {
		if (RuntimeEntity* e = ent(0)) {
			const std::string anim = S(1);
			e->track = anim;
			if (_resolve_track) {
				const int ti = _resolve_track(e->mbs, anim);
				if (ti >= 0)
					e->track_index = ti;
			}
			_dirty = true;
			const long dur = _track_frames ? _track_frames(e->mbs, e->track_index) : 0;
			if (dur > 0) {
				pending_wait = dur;
				if (asIScriptContext* ctx = asGetActiveContext())
					ctx->Suspend();
			}
		}
		return true;
	}
	if (name == "DrMoveAbsByTimeAsync") {
		float to[3] = {0.0f, 0.0f, 0.0f};
		V(1, to, 3);
		if (RuntimeEntity* e = ent(0)) {
			Move m{};
			for (int k = 0; k < 3; ++k) {
				m.from[k] = e->pos[k];
				m.to[k] = to[k];
			}
			m.f0 = frame;
			m.f1 = frame + std::max(1, I(3));
			_moves[e->name] = m;
			_dirty = true;
		}
		return true;
	}
	if (name == "DrShortMoveAbsAsync" || name == "DrShortMove") {
		float to[3] = {0.0f, 0.0f, 0.0f};
		V(1, to, 3);
		if (RuntimeEntity* e = ent(0)) {
			Move m{};
			float dist2 = 0.0f;
			for (int k = 0; k < 3; ++k) {
				m.from[k] = e->pos[k];
				m.to[k] = to[k];
				const float d = to[k] - e->pos[k];
				dist2 += d * d;
			}
			const float speed = F(3);
			const int dur = speed > 1e-3f ? static_cast<int>(std::ceil(std::sqrt(dist2) / speed)) : 30;
			m.f0 = frame;
			m.f1 = frame + std::max(1, dur);
			_moves[e->name] = m;
			_dirty = true;
			if (name == "DrShortMove") {
				pending_wait = std::max(1, dur);
				if (asIScriptContext* ctx = asGetActiveContext())
					ctx->Suspend();
			}
		}
		return true;
	}
	if (name == "DrJumpAsync" || name == "DrJump") {
		if (RuntimeEntity* e = ent(0)) {
			const float move = F(1), height = F(2), speed = F(4);
			Move m{};
			for (int k = 0; k < 3; ++k) {
				m.from[k] = e->pos[k];
				m.to[k] = e->pos[k];
			}
			m.to[0] += e->dir_x * move;
			const int dur = speed > 1e-3f ? static_cast<int>(std::ceil(std::abs(move) / speed)) : 30;
			m.f0 = frame;
			m.f1 = frame + std::max(1, dur);
			m.arc = std::abs(height);
			_moves[e->name] = m;
			_dirty = true;
			if (name == "DrJump") {
				pending_wait = std::max(1, dur);
				if (asIScriptContext* ctx = asGetActiveContext())
					ctx->Suspend();
			}
		}
		return true;
	}
	if (name == "DrTebureCamera") {
		_camera.tebure = I(0) != 0;
		_dirty = true;
		return true;
	}
	if (name == "DrResetCameraParam") {
		_camera.accel = 0.0f;
		_camera.decel = 0.0f;
		_dirty = true;
		return true;
	}
	if (name == "DrFollowCamera") {
		if (RuntimeEntity* e = ent(0)) {
			for (int k = 0; k < 3; ++k)
				_camera.pos[k] = e->pos[k];
			_camera.has_pos = true;
			_dirty = true;
		}
		const long wait = std::max(0, I(2));
		if (wait > 0) {
			pending_wait = wait;
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return true;
	}
	if (name == "DrCameraMoveWait") {
		if (_camera_move_end > frame) {
			pending_wait = _camera_move_end - frame;
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return true;
	}
	if (name == "DrSetShake" || name == "SetShake") {
		RuntimeShake s;
		s.start = frame;
		s.index = _shake_seq++;
		if (gen->GetArgTypeId(0) == asTYPEID_INT32) {
			s.type = I(0);
			s.end = frame + std::max(1, I(1));
		} else {
			float v[2] = {0.0f, 0.0f};
			V(0, v, 2);
			s.shake[0] = v[0];
			s.shake[1] = v[1];
			s.end = frame + std::max(1, I(1));
			s.angle = F(2);
			s.att = F(4);
			s.pow = F(6);
		}
		_timeline.shakes.push_back(s);
		gen->SetReturnDWord(static_cast<asDWORD>(s.index));
		return true;
	}
	if (name == "DrStopShake") {
		const int idx = I(0);
		for (auto& s : _timeline.shakes)
			if (s.index == idx && s.end > frame)
				s.end = frame;
		return true;
	}
	if (name == "BokashiFadeAsync") {
		RuntimeBokashi b;
		b.start = frame;
		b.end = frame + std::max(1, I(2));
		b.bokashi = I(0) != 0;
		b.freeze = I(1) != 0;
		b.max_rate = F(3);
		_timeline.bokashi.push_back(b);
		return true;
	}

	// --- 5A residuals: non-`Dr` aliases and synchronous movement/camera variants.
	if (name == "SetDemoCameraPosY") {
		_camera.demo_y = F(0);
		_dirty = true;
		return true;
	}
	if (name == "ResetDemoCameraPosY") {
		_camera.demo_y = 0.0f;
		_dirty = true;
		return true;
	}
	if (name == "ResetDemoCameraPosXZ") {
		_camera.demo_xz[0] = _camera.demo_xz[1] = 0.0f;
		_dirty = true;
		return true;
	}
	if (name == "SetCameraDegreeOffset") {
		_camera.degree_offset = F(0);
		_dirty = true;
		return true;
	}
	if (name == "FollowCamera" || name == "FollowCameraBySpeed") {
		if (RuntimeEntity* e = ent(0)) {
			for (int k = 0; k < 3; ++k)
				_camera.pos[k] = e->pos[k];
			_camera.has_pos = true;
			_dirty = true;
		}
		const long wait = std::max(0, I(2));
		if (wait > 0) {
			pending_wait = wait;
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return true;
	}
	if (name == "FollowDirClear") {
		if (RuntimeEntity* e = ent(0)) {
			e->dir_x = 1.0f;
			_dirty = true;
		}
		return true;
	}
	if (name == "FollowOffsetV") {
		float v[2] = {0.0f, 0.0f};
		V(0, v, 2);
		_follow_offset[0] = v[0];
		_follow_offset[1] = v[1];
		return true;
	}
	if (name == "ResetCameraParam") {
		_camera.accel = _camera.decel = 0.0f;
		_dirty = true;
		return true;
	}
	if (name == "DrCameraStop") {
		_camera_move_end = frame;
		return true;
	}
	if (name == "DrWaveCamera") {
		_camera.wave = I(0) != 0;
		_dirty = true;
		return true;
	}
	if (name == "DrResetCameraZoomParam")
		return true;
	if (name == "DrStopShakeFadeoutAll") {
		for (auto& s : _timeline.shakes)
			if (s.end > frame)
				s.end = frame;
		return true;
	}
	if (name == "DrCopyCharaInfo") {
		if (RuntimeEntity* dst = ent(1)) {
			if (RuntimeEntity* src = ent(0)) {
				dst->mbs = src->mbs;
				dst->ftx = src->ftx;
				dst->kind = src->kind;
				_dirty = true;
			}
		}
		return true;
	}
	if (name == "DrEnableBalconyFlagBRU") {
		_balcony_bru = true;
		return true;
	}
	if (name == "DrEnableBalconyFlagSCR") {
		_balcony_scr = true;
		return true;
	}
	if (name == "DrGetBalconyFlagBRU") {
		gen->SetReturnByte(_balcony_bru ? 1 : 0);
		return true;
	}
	if (name == "DrGetBalconyFlagSCR") {
		gen->SetReturnByte(_balcony_scr ? 1 : 0);
		return true;
	}
	if (name == "CancelAllSkipMode") {
		_skip_mode = false;
		return true;
	}

	// Synchronous 2D movement variants. `MoveBack` is modelled as a negative
	// relative delta; the rest are absolute. `ByTime` variants block.
	auto move2 = [&](int pos_arg, int time_arg, bool absolute, bool negative) -> bool {
		float to[3] = {0.0f, 0.0f, 0.0f};
		V(pos_arg, to, 2);
		if (RuntimeEntity* e = ent(0)) {
			Move m{};
			for (int k = 0; k < 3; ++k) {
				m.from[k] = e->pos[k];
				m.to[k] = absolute ? to[k] : e->pos[k] + (negative ? -to[k] : to[k]);
			}
			m.to[2] = e->pos[2];
			m.f0 = frame;
			m.f1 = frame + std::max(1, I(time_arg));
			_moves[e->name] = m;
			_dirty = true;
			pending_wait = m.f1 - frame;
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return true;
	};
	if (name == "MoveAbsByTime" || name == "MoveBackAbsByTime")
		return move2(1, 2, true, false);
	if (name == "MoveBackAbs")
		return move2(1, 2, true, false);
	if (name == "MoveBack")
		return move2(1, 2, false, true);
	if (name == "ObjectDeleteWait")
		return true;
	if (name == "SetDemoClipRange")
		return true;
	if (name == "TutorialFadeAsync") {
		float c[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		read_color(gen, 0, c);
		push_fade(c, I(1));
		return true;
	}
	if (name == "WaitAnyButton" || name == "WaitOKButton")
		return true; // input waits cannot be satisfied headlessly

	// --- 5B: record-only effects, lights, audio, item spawns.
	auto to_bytes = [](const float* c, unsigned char* o) {
		for (int k = 0; k < 4; ++k)
			o[k] = static_cast<unsigned char>(std::clamp(c[k], 0.0f, 1.0f) * 255.0f);
	};

	if (name == "DrCreateEffect") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "create";
		e.name = S(0);
		e.mbs = S(1);
		e.ftx = S(2);
		e.anime = I(3);
		V(4, e.pos, 3);
		e.i0 = I(5);
		e.f0 = F(6);
		e.flag = I(7) != 0;
		e.i1 = I(8);
		e.i2 = I(9);
		_timeline.effects.push_back(e);
		return true;
	}
	if (name == "DrSetEffectDirX") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "dir";
		e.name = S(0);
		e.i0 = I(1);
		_timeline.effects.push_back(e);
		return true;
	}
	if (name == "DrSetEffectCharaCollision") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "collision";
		e.name = S(0);
		e.attached = S(1);
		e.i0 = I(2);
		e.i1 = I(3);
		_timeline.effects.push_back(e);
		return true;
	}
	if (name == "DrSetCharaToEffect") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "attach";
		e.name = S(0);
		e.attached = S(1);
		_timeline.effects.push_back(e);
		return true;
	}
	if (name == "SetDrCharaToSpriteEffect") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "sprite_attach";
		e.name = S(0);
		e.attached = S(1);
		V(2, e.pos, 2);
		_timeline.effects.push_back(e);
		return true;
	}
	if (name == "DrRotateEffectAsync") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "rotate";
		e.name = S(0);
		e.f0 = F(1);
		e.i0 = I(2);
		_timeline.effects.push_back(e);
		return true;
	}
	if (name == "DrMoveEffect" || name == "DrMoveEffectAsync") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "move";
		e.name = S(0);
		V(1, e.pos, 3);
		e.i0 = I(2);
		e.i1 = I(3);
		e.i2 = I(4);
		_timeline.effects.push_back(e);
		return true;
	}
	if (name == "DrScaleEffectAsync") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "scale";
		e.name = S(0);
		e.f0 = F(1);
		e.i0 = I(2);
		_timeline.effects.push_back(e);
		return true;
	}
	if (name == "ExecOpeningEffect") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "opening";
		_timeline.effects.push_back(e);
		return true;
	}
	if (name == "DrCreatePointLight") {
		RuntimePointLight l;
		l.frame = frame;
		l.name = S(0);
		V(1, l.pos, 3);
		float c[4] = {0, 0, 0, 0};
		read_color(gen, 2, c);
		to_bytes(c, l.light);
		read_color(gen, 3, c);
		to_bytes(c, l.shadow);
		read_color(gen, 4, c);
		to_bytes(c, l.godray);
		l.power = F(5);
		l.bloom = F(6);
		l.f2 = F(7);
		l.f3 = F(8);
		l.f4 = F(9);
		l.f5 = F(10);
		_timeline.point_lights.push_back(l);
		return true;
	}
	if (name == "DrDeletePointLight") {
		RuntimePointLight l;
		l.frame = frame;
		l.remove = true;
		l.name = S(0);
		l.index = I(1);
		_timeline.point_lights.push_back(l);
		return true;
	}
	if (name == "DrSetCharaToPointLight") {
		RuntimePointLight l;
		l.frame = frame;
		l.name = S(0);
		l.attached = S(1);
		V(2, l.pos, 3);
		_timeline.point_lights.push_back(l);
		return true;
	}
	if (name == "DrSetPointLightCharaCollision") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "pointlight_collision";
		e.name = S(0);
		e.attached = S(1);
		e.i0 = I(2);
		e.i1 = I(3);
		_timeline.effects.push_back(e);
		return true;
	}
	if (name == "DrSetShadowOff") {
		if (RuntimeEntity* e = ent(0)) {
			e->shadow_off = I(1) != 0;
			_dirty = true;
		}
		return true;
	}
	if (name == "DrSetSortIDOffset") {
		if (RuntimeEntity* e = ent(0)) {
			e->sort_id_offset = I(1);
			_dirty = true;
		}
		return true;
	}
	if (name == "DrSetDLPrio") {
		if (RuntimeEntity* e = ent(0)) {
			e->dlprio = I(1);
			_dirty = true;
		}
		return true;
	}
	if (name == "DrSetWindowIDOffset") {
		if (RuntimeEntity* e = ent(0)) {
			e->window_id_offset = I(1);
			_dirty = true;
		}
		return true;
	}
	if (name == "DrSetPrerenderOnForce") {
		if (RuntimeEntity* e = ent(0)) {
			e->prerender_force = I(1) != 0;
			_dirty = true;
		}
		return true;
	}
	if (name == "DrSetCharaColor") {
		if (RuntimeEntity* e = ent(0)) {
			for (int k = 0; k < 4; ++k)
				e->color[k] = I(1 + k);
			_dirty = true;
		}
		return true;
	}
	if (name == "DrSetFogColor") {
		float c[4] = {0, 0, 0, 0};
		read_color(gen, 1, c);
		RuntimeFog f;
		f.frame = frame;
		f.name = S(0);
		to_bytes(c, f.color);
		_timeline.fog.push_back(f);
		return true;
	}
	if (name == "DrSetGradientMap") {
		_timeline.gradients.push_back(RuntimeGradient{frame, I(0), F(1), I(2)});
		return true;
	}
	if (name == "BokashiFadeStop") {
		if (!_timeline.bokashi.empty() && _timeline.bokashi.back().end > frame)
			_timeline.bokashi.back().end = frame;
		return true;
	}

	auto push_audio = [&](std::string kind, int cue, std::string target, int fade, int wait, int index, float vol) -> bool {
		RuntimeAudio a;
		a.frame = frame;
		a.kind = std::move(kind);
		a.cue = cue;
		a.target = std::move(target);
		a.fade = fade;
		a.wait = wait;
		a.index = index;
		a.volume = vol;
		_timeline.audio.push_back(std::move(a));
		if (wait > 0) {
			pending_wait = wait;
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return true;
	};
	if (name == "PlayBGM")
		return push_audio("bgm", I(0), {}, I(1), I(2), I(3), 0.0f);
	if (name == "PlayDefaultBGM")
		return push_audio("bgm_default", 0, {}, I(0), I(1), I(2), 0.0f);
	if (name == "PlaySE")
		return push_audio("se", I(0), {}, 0, I(1), -1, 0.0f);
	if (name == "PlaySE_Chara")
		return push_audio("se_chara", I(0), S(1), 0, I(2), -1, 0.0f);
	if (name == "StopBGM")
		return push_audio("stop_bgm", 0, {}, I(0), I(1), -1, 0.0f);
	if (name == "StopSE")
		return push_audio("stop_se", I(0), {}, I(1), 0, -1, 0.0f);
	if (name == "SetBGMVolume")
		return push_audio("bgm_volume", 0, {}, I(1), 0, -1, F(0));
	if (name == "SetSeVolume")
		return push_audio("se_volume", 0, {}, I(1), 0, -1, F(0));
	if (name == "SetAmbVolume")
		return push_audio("amb_volume", 0, {}, I(1), 0, -1, F(0));
	if (name == "AmbVolumeSyncByFade")
		return push_audio("amb_fade", 0, {}, 0, 0, -1, F(0));
	if (name == "SetStageBGM")
		return push_audio("stage_bgm", I(0), {}, 0, 0, -1, 0.0f);
	if (name == "PlayBGMWithTime")
		return push_audio("bgm_time", I(0), {}, I(1), I(2), I(3), 0.0f);
	if (name == "PlayActionCue")
		return push_audio("action_cue", I(0), {}, 0, 0, -1, 0.0f);
	if (name == "DrSetThunderVolumeLevel")
		return push_audio("thunder", 0, {}, 0, 0, -1, static_cast<float>(I(0)));
	if (name == "SetBattleBGM") {
		if (gen->GetArgTypeId(0) == asTYPEID_INT32)
			return push_audio("battle_bgm", I(0), {}, 0, 0, -1, 0.0f);
		return push_audio("battle_bgm", 0, S(0), I(1), 0, -1, 0.0f);
	}
	if (name == "UC_SEC") {
		// seconds -> frames at 60 fps (scripts use it as Wait(UC_SEC(x))).
		gen->SetReturnDWord(static_cast<asDWORD>(static_cast<int>(F(0) * 60.0f)));
		return true;
	}
	if (name == "GetSoundLength") {
		// TODO: return the real cue length in frames. The ACB/AFS2 parser
		// (libcriware/src/acb.cpp) has the cue -> waveform mapping; scripts use
		// Wait(GetSoundLength(cue) + 60). Returning 0 is a timing approximation.
		gen->SetReturnDWord(0);
		return true;
	}

	if (name == "CreateItem") {
		RuntimeItem it;
		it.frame = frame;
		it.kind = "create";
		if (gen->GetArgTypeId(0) == asTYPEID_INT32) {
			it.id = I(0);
			it.dialog = I(1) != 0;
		} else {
			it.item = S(0);
			it.count = I(1);
			it.dialog = I(2) != 0;
		}
		_timeline.items.push_back(it);
		return true;
	}
	if (name == "DeleteItem") {
		RuntimeItem it;
		it.frame = frame;
		it.kind = "delete";
		if (gen->GetArgTypeId(0) == asTYPEID_INT32) {
			it.id = I(0);
			it.count = I(1);
			it.dialog = I(2) != 0;
		} else {
			it.item = S(0);
			it.count = I(1);
			it.dialog = I(2) != 0;
		}
		_timeline.items.push_back(it);
		return true;
	}
	if (name == "CreateDbItem") {
		RuntimeItem it;
		it.frame = frame;
		it.kind = "db";
		it.id = I(0);
		const auto add = read_int_array(gen, 1);
		it.add_count = static_cast<int>(std::min<size_t>(add.size(), 8));
		for (int k = 0; k < it.add_count; ++k)
			it.add[k] = add[k];
		_timeline.items.push_back(it);
		return true;
	}
	if (name == "CreateTreasure") {
		RuntimeItem it;
		it.frame = frame;
		it.kind = "treasure";
		it.object = S(0);
		V(1, it.pos, 2);
		if (gen->GetArgTypeId(2) == asTYPEID_INT32) {
			it.id = I(2);
			if (gen->GetArgCount() > 3)
				it.callback = S(3);
		} else {
			it.item = S(2);
			it.count = I(3);
			if (gen->GetArgCount() > 4)
				it.callback = S(4);
		}
		_timeline.items.push_back(it);
		return true;
	}

	// TODO(blocked natives): the groups below fall through to the GenericStub,
	// which returns the type's zero (never crashes). They are unimplemented
	// on purpose, each blocked on a prerequisite subsystem:
	//  - AI / strategy: StAddUnitAi*/Gambit/SearchEnemy, StIsLowerHpRate, StAdd*Zouen*
	//    -> Phase 7: coarse RE for the gist, then invent the battle simulation.
	//  - Effects / audio / spawns: DrCreateEffect, DrSetEffect*, DrCreatePointLight/
	//    DeletePointLight, PlayBGM/StopBGM/PlaySE*, CreateItem/DeleteItem/CreateTreasure
	//    -> Phase 5B (record) and Phase 8 (effect/lighting render model).
	//  - Field plumbing: DrCreateDrama, ReconstTown -> Phase 6.
	//  - DrSetShake(int) preset table -> Phase 8 (visual/timing only; the
	//    structured overload and the int-overload handle are modelled).
	//  - UI / archive / HUD: OpenArchive, ShowFacilityInfo, ShowFieldInfo,
	//    SetClassPedia_ByDefName, ... -> Phase 5C.
	//  - Time conversion: UC_SEC -> game-time formula unconfirmed.

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

void Registry::flush_keyframe() {
	// Advance active movement tweens, driving the entity positions.
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
		float* p = _entities[bi->second].pos;
		for (int k = 0; k < 3; ++k)
			p[k] = m.from[k] + (m.to[k] - m.from[k]) * t;
		if (m.arc != 0.0f)
			p[2] += m.arc * 4.0f * t * (1.0f - t);
		_dirty = true;
		if (frame >= m.f1)
			it = _moves.erase(it);
		else
			++it;
	}
	if (!_dirty && !_timeline.keyframes.empty())
		return;
	Keyframe k;
	k.frame = frame;
	k.entities = _entities;
	k.camera = _camera;
	_timeline.keyframes.push_back(std::move(k));
	_dirty = false;
}

} // namespace asb
