#include "native_args.hpp"
#include "registry.hpp"

#include <angelscript.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <vector>

namespace asb {

namespace {

void to_bytes(const float* color, unsigned char* out) {
	for (int i = 0; i < 4; ++i)
		out[i] = static_cast<unsigned char>(std::clamp(color[i], 0.0f, 1.0f) * 255.0f);
}

} // namespace

DispatchResult Registry::dispatch_effects(std::string_view name, NativeArgs& args) {
	auto* gen = args.generic();
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

	// Record-only effects, lights, audio, and item spawns.
	if (name == "DrSetEffectPosV" || name == "DrSetEffectOffsetV") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = name == "DrSetEffectPosV" ? "pos" : "offset";
		e.name = S(0);
		V(1, e.pos, 3);
		_timeline.effects.push_back(e);
		return DispatchResult::Handled;
	}
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
		return DispatchResult::Handled;
	}
	if (name == "DrSetEffectDirX") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "dir";
		e.name = S(0);
		e.i0 = I(1);
		_timeline.effects.push_back(e);
		return DispatchResult::Handled;
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
		return DispatchResult::Handled;
	}
	if (name == "DrSetCharaToEffect") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "attach";
		e.name = S(0);
		e.attached = S(1);
		_timeline.effects.push_back(e);
		return DispatchResult::Handled;
	}
	if (name == "SetDrCharaToSpriteEffect") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "sprite_attach";
		e.name = S(0);
		e.attached = S(1);
		V(2, e.pos, 2);
		_timeline.effects.push_back(e);
		return DispatchResult::Handled;
	}
	if (name == "DrRotateEffectAsync") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "rotate";
		e.name = S(0);
		e.f0 = F(1);
		e.i0 = I(2);
		_timeline.effects.push_back(e);
		return DispatchResult::Handled;
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
		return DispatchResult::Handled;
	}
	if (name == "DrScaleEffectAsync") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "scale";
		e.name = S(0);
		e.f0 = F(1);
		e.i0 = I(2);
		_timeline.effects.push_back(e);
		return DispatchResult::Handled;
	}
	if (name == "ExecOpeningEffect") {
		RuntimeEffect e;
		e.frame = frame;
		e.kind = "opening";
		_timeline.effects.push_back(e);
		return DispatchResult::Handled;
	}
	if (name == "DrCreatePointLight") {
		RuntimePointLight l;
		l.frame = frame;
		l.name = S(0);
		V(1, l.pos, 3);
		float c[4] = {0, 0, 0, 0};
		args.color(2, c);
		to_bytes(c, l.light);
		args.color(3, c);
		to_bytes(c, l.shadow);
		args.color(4, c);
		to_bytes(c, l.godray);
		l.power = F(5);
		l.bloom = F(6);
		l.f2 = F(7);
		l.f3 = F(8);
		l.f4 = F(9);
		l.f5 = F(10);
		_timeline.point_lights.push_back(l);
		return DispatchResult::Handled;
	}
	if (name == "DrDeletePointLight") {
		RuntimePointLight l;
		l.frame = frame;
		l.remove = true;
		l.name = S(0);
		l.index = I(1);
		_timeline.point_lights.push_back(l);
		return DispatchResult::Handled;
	}
	if (name == "DrSetCharaToPointLight") {
		RuntimePointLight l;
		l.frame = frame;
		l.name = S(0);
		l.attached = S(1);
		V(2, l.pos, 3);
		_timeline.point_lights.push_back(l);
		return DispatchResult::Handled;
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
		return DispatchResult::Handled;
	}
	if (name == "DrSetShadowOff") {
		if (RuntimeEntity* e = ent(0)) {
			e->shadow_off = I(1) != 0;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrSetSortIDOffset") {
		if (RuntimeEntity* e = ent(0)) {
			e->sort_id_offset = I(1);
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrSetDLPrio") {
		if (RuntimeEntity* e = ent(0)) {
			e->dlprio = I(1);
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrSetWindowIDOffset") {
		if (RuntimeEntity* e = ent(0)) {
			e->window_id_offset = I(1);
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrSetPrerenderOnForce") {
		if (RuntimeEntity* e = ent(0)) {
			e->prerender_force = I(1) != 0;
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrSetCharaColor") {
		if (RuntimeEntity* e = ent(0)) {
			for (int k = 0; k < 4; ++k)
				e->color[k] = I(1 + k);
			mark_dirty();
		}
		return DispatchResult::Handled;
	}
	if (name == "DrSetFogColor") {
		float c[4] = {0, 0, 0, 0};
		args.color(1, c);
		RuntimeFog f;
		f.frame = frame;
		f.name = S(0);
		to_bytes(c, f.color);
		_timeline.fog.push_back(f);
		return DispatchResult::Handled;
	}
	if (name == "DrSetGradientMap") {
		_timeline.gradients.push_back(RuntimeGradient{frame, I(0), F(1), I(2)});
		return DispatchResult::Handled;
	}
	if (name == "BokashiFadeStop") {
		if (!_timeline.bokashi.empty() && _timeline.bokashi.back().end > frame)
			_timeline.bokashi.back().end = frame;
		return DispatchResult::Handled;
	}

	auto push_audio =
		[&](std::string kind, int cue, std::string target, int fade, int wait, int index, float vol) -> bool {
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
	if (name == "PlayBGM") {
		push_audio("bgm", I(0), {}, I(1), I(2), I(3), 0.0f);
		return DispatchResult::Handled;
	}
	if (name == "PlayDefaultBGM") {
		push_audio("bgm_default", 0, {}, I(0), I(1), I(2), 0.0f);
		return DispatchResult::Handled;
	}
	if (name == "PlaySE") {
		push_audio("se", I(0), {}, 0, I(1), -1, 0.0f);
		return DispatchResult::Handled;
	}
	if (name == "PlaySE_Chara") {
		push_audio("se_chara", I(0), S(1), 0, I(2), -1, 0.0f);
		return DispatchResult::Handled;
	}
	if (name == "StopBGM") {
		push_audio("stop_bgm", 0, {}, I(0), I(1), -1, 0.0f);
		return DispatchResult::Handled;
	}
	if (name == "StopSE") {
		push_audio("stop_se", I(0), {}, I(1), 0, -1, 0.0f);
		return DispatchResult::Handled;
	}
	if (name == "SetBGMVolume") {
		push_audio("bgm_volume", 0, {}, I(1), 0, -1, F(0));
		return DispatchResult::Handled;
	}
	if (name == "SetSeVolume") {
		push_audio("se_volume", 0, {}, I(1), 0, -1, F(0));
		return DispatchResult::Handled;
	}
	if (name == "SetAmbVolume") {
		push_audio("amb_volume", 0, {}, I(1), 0, -1, F(0));
		return DispatchResult::Handled;
	}
	if (name == "AmbVolumeSyncByFade") {
		push_audio("amb_fade", 0, {}, 0, 0, -1, F(0));
		return DispatchResult::Handled;
	}
	if (name == "SetStageBGM") {
		push_audio("stage_bgm", I(0), {}, 0, 0, -1, 0.0f);
		return DispatchResult::Handled;
	}
	if (name == "PlayBGMWithTime") {
		push_audio("bgm_time", I(0), {}, I(1), I(2), I(3), 0.0f);
		return DispatchResult::Handled;
	}
	if (name == "PlayActionCue") {
		push_audio("action_cue", I(0), {}, 0, 0, -1, 0.0f);
		return DispatchResult::Handled;
	}
	if (name == "DrSetThunderVolumeLevel") {
		push_audio("thunder", 0, {}, 0, 0, -1, static_cast<float>(I(0)));
		return DispatchResult::Handled;
	}
	if (name == "SetBattleBGM") {
		if (gen->GetArgTypeId(0) == asTYPEID_INT32)
			push_audio("battle_bgm", I(0), {}, 0, 0, -1, 0.0f);
		else
			push_audio("battle_bgm", 0, S(0), I(1), 0, -1, 0.0f);
		return DispatchResult::Handled;
	}
	if (name == "UC_SEC") {
		// TODO(timing): seconds -> frames at 60 fps (inferred from sound padding).
		gen->SetReturnDWord(static_cast<asDWORD>(static_cast<int>(F(0) * 60.0f)));
		return DispatchResult::Handled;
	}
	if (name == "GetSoundLength") {
		// Real cue length (frames) from the ACB cue/waveform tables when the host
		// supplies a resolver; 0 is the timing fallback. Actual audio playback is
		// still TODO (we only need the length for Wait(GetSoundLength(cue)+60)).
		const int cue = I(0);
		const int len = _sound_length ? _sound_length(cue) : 0;
		spdlog::debug("sound: GetSoundLength({})={}", cue, len);
		gen->SetReturnDWord(static_cast<asDWORD>(len));
		return DispatchResult::Handled;
	}

	if (name == "CreateItem") {
		RuntimeItem it;
		it.frame = frame;
		it.kind = "create";
		if (gen->GetArgTypeId(0) == asTYPEID_INT32) {
			// Two numeric overloads: (itemID, count, dialog) and (dropTableID, dialog).
			const bool drop = gen->GetArgCount() >= 2 && gen->GetArgTypeId(1) == asTYPEID_BOOL;
			if (drop) {
				it.kind = "drop";
				it.id = I(0);
				it.dialog = I(1) != 0;
				const auto* entries = _drops ? _drops->resolve(it.id) : nullptr;
				if (entries && !entries->empty()) {
					for (const auto& e : *entries) {
						RuntimeItem di = it;
						di.item = e.item;
						di.count = e.count;
						save().items[di.item] += di.count;
						_timeline.items.push_back(std::move(di));
					}
					return DispatchResult::Handled;
				}
			} else {
				it.id = I(0);
				it.count = gen->GetArgCount() >= 2 ? I(1) : 1;
				it.dialog = gen->GetArgCount() >= 3 ? I(2) != 0 : true;
				if (_drops)
					it.item = _drops->item_name(it.id);
				if (!it.item.empty())
					save().items[it.item] += it.count;
			}
		} else {
			it.item = S(0);
			it.count = gen->GetArgCount() >= 2 ? I(1) : 1;
			it.dialog = gen->GetArgCount() >= 3 ? I(2) != 0 : true;
			save().items[it.item] += it.count;
		}
		_timeline.items.push_back(it);
		return DispatchResult::Handled;
	}
	if (name == "DeleteItem") {
		RuntimeItem it;
		it.frame = frame;
		it.kind = "delete";
		if (gen->GetArgTypeId(0) == asTYPEID_INT32) {
			it.id = I(0);
			it.count = gen->GetArgCount() >= 2 ? I(1) : 1;
			it.dialog = gen->GetArgCount() >= 3 ? I(2) != 0 : true;
			if (_drops)
				it.item = _drops->item_name(it.id);
		} else {
			it.item = S(0);
			it.count = gen->GetArgCount() >= 2 ? I(1) : 1;
			it.dialog = gen->GetArgCount() >= 3 ? I(2) != 0 : true;
		}
		if (!it.item.empty()) {
			auto q = save().items.find(it.item);
			if (q != save().items.end())
				q->second = std::max(0, q->second - it.count);
		}
		_timeline.items.push_back(it);
		return DispatchResult::Handled;
	}
	if (name == "CreateDbItem") {
		RuntimeItem it;
		it.frame = frame;
		it.kind = "db";
		it.id = I(0);
		const auto add = args.ints(1);
		it.add_count = static_cast<int>(std::min<size_t>(add.size(), 8));
		for (int k = 0; k < it.add_count; ++k)
			it.add[k] = add[k];
		const auto* entries = _drops ? _drops->resolve(it.id) : nullptr;
		if (entries && !entries->empty()) {
			for (const auto& e : *entries) {
				// `_aAddNum` is indexed by the entry's 1-based `add_index`.
				const int idx = e.add_index - 1;
				const int extra = idx >= 0 && static_cast<size_t>(idx) < add.size() ? add[idx] : 0;
				RuntimeItem di = it;
				di.item = e.item;
				di.count = std::max(0, e.count + extra);
				di.add_count = 0;
				save().items[di.item] += di.count;
				_timeline.items.push_back(std::move(di));
			}
			return DispatchResult::Handled;
		}
		_timeline.items.push_back(it);
		return DispatchResult::Handled;
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
		if (!it.callback.empty())
			add_callback(ScriptEvent::Treasure, it.object, it.callback, true);
		_timeline.items.push_back(it);
		return DispatchResult::Handled;
	}

	if (name == "DrSetShake" || name == "SetShake") {
		RuntimeShake s;
		s.start = frame;
		s.index = _shake_seq++;
		if (gen->GetArgTypeId(0) == asTYPEID_INT32) {
			// Engine preset table 0x28a5388/0x28a5390 + the 14-type jump table at
			// 0xaf24c0. The table supplies a direction (1,0)/(0,1), an amplitude
			// (12/24/45/60) and a time (4/8/15/20); 11..13 use 2/5/8. The exact
			// envelope is approximated here and refined by the viewer. TODO(shake).
			const int type = I(0);
			const int wait = gen->GetArgCount() > 1 ? I(1) : 0;
			static const int amp[4] = {12, 24, 45, 60};
			static const int dur[4] = {4, 8, 15, 20};
			static const int short_dur[3] = {2, 5, 8};
			s.type = type;
			s.start = frame + std::max(0, wait);
			s.att = 1.0f;
			s.pow = 1.0f;
			if (type >= 0 && type <= 3) {
				s.shake[0] = static_cast<float>(amp[type]);
				s.end = s.start + dur[type];
			} else if (type >= 5 && type <= 8) {
				s.shake[1] = static_cast<float>(amp[type - 5]);
				s.end = s.start + dur[type - 5];
			} else if (type == 4 || type == 9) {
				const int k = type == 4 ? 0 : 3;
				s.shake[0] = static_cast<float>(amp[k]);
				s.end = s.start + dur[k];
			} else if (type >= 11 && type <= 13) {
				s.shake[0] = 1.0f;
				s.end = s.start + short_dur[type - 11];
			} else {
				s.end = s.start + std::max(1, wait);
			}
			if (s.end <= s.start)
				s.end = s.start + 1;
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
		return DispatchResult::Handled;
	}
	if (name == "DrStopShake") {
		const int idx = I(0);
		for (auto& s : _timeline.shakes)
			if (s.index == idx && s.end > frame)
				s.end = frame;
		return DispatchResult::Handled;
	}
	if (name == "BokashiFadeAsync") {
		RuntimeBokashi b;
		b.start = frame;
		b.end = frame + std::max(1, I(2));
		b.bokashi = I(0) != 0;
		b.freeze = I(1) != 0;
		b.max_rate = F(3);
		_timeline.bokashi.push_back(b);
		return DispatchResult::Handled;
	}
	if (name == "StopShakeFadeoutAll" || name == "DrStopShakeFadeoutAll") {
		for (auto& shake : _timeline.shakes)
			if (shake.end > frame)
				shake.end = frame;
		return DispatchResult::Handled;
	}
	return DispatchResult::Unhandled;
}

} // namespace asb
