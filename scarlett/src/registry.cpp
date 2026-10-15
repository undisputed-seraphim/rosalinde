#include "registry.hpp"

#include <angelscript.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace asb {

namespace {

std::string read_string(asIScriptGeneric* gen, int i) {
	auto* p = static_cast<const std::string*>(gen->GetArgAddress(i));
	return p ? *p : std::string();
}

void read_vec(asIScriptGeneric* gen, int i, float* out, int n) {
	auto* p = static_cast<const float*>(gen->GetArgAddress(i));
	for (int k = 0; k < n; ++k)
		out[k] = p ? p[k] : 0.0f;
}

bool plausible_scale(float s) { return std::isfinite(s) && s > 1e-4f && s < 1e4f; }

} // namespace

void Registry::reset(std::string source, std::string function) {
	_timeline = Timeline{};
	_timeline.source = std::move(source);
	_timeline.function = std::move(function);
	_entities.clear();
	_by_name.clear();
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
	int idx = ensure(id);
	auto& e = _entities[idx];
	e.kind = kind;
	e.space = kind == "DrUiCreateChara" ? RuntimeEntity::Space::Ui : RuntimeEntity::Space::World;
	e.aliases = names;
	if (_chars) {
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
	asITypeInfo* ot = fn ? fn->GetObjectType() : nullptr;
	void* obj = gen->GetObject();
	if (!ot || !obj)
		return false;
	std::string on = ot->GetName();
	bool is2 = on == "FnVector2", is3 = on == "FnVector3", is4 = on == "FnVector4";
	if (!is2 && !is3 && !is4)
		return false;
	float* out = static_cast<float*>(obj);
	const int comps = is2 ? 2 : (is3 ? 3 : 4);
	for (int i = 0; i < comps; ++i)
		out[i] = 0.0f;
	const int n = static_cast<int>(fn->GetParamCount());
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
	float* r = static_cast<float*>(gen->GetAddressOfReturnLocation());
	if (!a || !r)
		return false;
	const bool scalar = gen->GetArgTypeId(0) == asTYPEID_FLOAT;
	const float s = scalar ? gen->GetArgFloat(0) : 0.0f;
	const float* b = scalar ? nullptr : static_cast<const float*>(gen->GetArgAddress(0));
	for (int i = 0; i < comps; ++i) {
		const float y = scalar ? s : (b ? b[i] : 0.0f);
		if (op == "opAdd")
			r[i] = a[i] + y;
		else if (op == "opSub")
			r[i] = a[i] - y;
		else if (op == "opMul")
			r[i] = a[i] * y;
		else if (op == "opDiv")
			r[i] = y != 0.0f ? a[i] / y : 0.0f;
	}
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
		if (asIScriptContext* ctx = asGetActiveContext())
			ctx->Suspend();
		return true;
	}
	if (name == "WaitSpriteMoneAnimationEnd" || name == "WaitFadeDone" || name == "DrActionWait" ||
		name == "ObjectSetupWait")
		return true; // modelled as immediate while the engine action has no runtime

	if (name.size() >= 4 && name.substr(0, 4) == "$beh")
		return construct_value(gen);

	if ((name == "opAdd" || name == "opSub" || name == "opMul" || name == "opDiv") && vec_binary(gen, name))
		return true;

	auto S = [&](int i) { return read_string(gen, i); };
	auto F = [&](int i) { return gen->GetArgFloat(i); };
	auto I = [&](int i) { return static_cast<int>(gen->GetArgDWord(i)); };
	auto V = [&](int i, float* o, int n) { read_vec(gen, i, o, n); };
	auto ent = [&](int i) -> RuntimeEntity* {
		auto it = _by_name.find(read_string(gen, i));
		return it == _by_name.end() ? nullptr : &_entities[it->second];
	};

	if (name == "CreateSpriteMone" || name == "CreateSpriteEffect" || name == "CreateSpriteMoneWorld") {
		float p[3] = {0.0f, 0.0f, 0.0f};
		V(4, p, 2);
		create_sprite(std::string(name), S(2), S(0), S(1), p, I(5), F(6));
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

	if (name == "SetSpriteMoneAnimation" || name == "SetSpriteMoneAnimationPaused") {
		if (RuntimeEntity* e = ent(0)) {
			e->track_index = I(1);
			_dirty = true;
		}
		return true;
	}
	if (name == "DrUiSetAnimation") {
		if (RuntimeEntity* e = ent(0)) {
			e->track_index = I(1);
			_dirty = true;
		}
		return true;
	}
	if (name == "DrSetIdleAnimation" || name == "DrAnimation" || name == "DrChangeAnimation") {
		if (RuntimeEntity* e = ent(0)) {
			e->track = S(1);
			_dirty = true;
		}
		return true;
	}

	if (name == "DrUiSetDirX" || name == "SetSpriteDirX") {
		if (RuntimeEntity* e = ent(0)) {
			e->dir_x = I(1) < 0 ? -1.0f : 1.0f;
			_dirty = true;
		}
		return true;
	}

	// DrUiSetColorA(name, value, time): the value is the target grey/alpha (0 at
	// creation, 255 when fully faded in); fade time is ignored. Track the low
	// byte so a not-yet-faded-in sprite stays hidden.
	if (name == "DrUiSetColorA" || name == "SetSpriteColorA" || name == "DrSetColorA") {
		if (RuntimeEntity* e = ent(0)) {
			e->alpha = I(1) & 0xff;
			_dirty = true;
		}
		return true;
	}

	if (name == "DeleteSprite" || name == "DrUiDeleteChara" || name == "DrDeleteEffect" || name == "DeleteEffect") {
		if (RuntimeEntity* e = ent(0)) {
			e->visible = false;
			_dirty = true;
		}
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

	// Unmodelled call. With ASB_CENSUS set, report it so a run can enumerate the
	// natives a scene depends on (returns stay at their stub defaults).
	if (std::getenv("ASB_CENSUS"))
		std::fprintf(stderr, "[census] %s%s%s\n", fn->GetNamespace(), *fn->GetNamespace() ? "::" : "", fn->GetName());

	return false;
}

void Registry::flush_keyframe() {
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
