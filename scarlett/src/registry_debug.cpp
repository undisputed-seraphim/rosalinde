#include "native_args.hpp"
#include "registry.hpp"

#include <angelscript.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <new>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace asb {

namespace {

unsigned char color_byte(float value) { return static_cast<unsigned char>(std::clamp(value * 255.0f, 0.0f, 255.0f)); }

} // namespace

DispatchResult Registry::dispatch_debug(std::string_view name, NativeArgs& args) {
	asIScriptGeneric* gen = args.generic();
	asIScriptFunction* fn = gen->GetFunction();
	auto S = [&](int i) { return args.str(i); };
	auto I = [&](int i) { return args.i32(i); };
	if (name == "DbIsDebugDrama") {
		gen->SetReturnByte(0);
		return DispatchResult::Handled;
	}
	if (name == "DbSetDebugParam") {
		_db_params[S(0)] = I(1);
		return DispatchResult::Handled;
	}
	if (name == "DbGetDebugParam") {
		auto it = _db_params.find(S(0));
		gen->SetReturnDWord(it == _db_params.end() ? 0u : static_cast<asDWORD>(it->second));
		return DispatchResult::Handled;
	}
	if (name == "DbGetLocalParam") {
		auto it = _db_params.find(S(0));
		gen->SetReturnFloat(it == _db_params.end() ? 0.0f : static_cast<float>(it->second));
		return DispatchResult::Handled;
	}
	if (name == "AddToHelpList") {
		_help.insert(S(0));
		return DispatchResult::Handled;
	}
	if (name == "ShowTips")
		return DispatchResult::Handled; // UI tip hint: no runtime model

	{
		asITypeInfo* ot = fn->GetObjectType();
		const bool vec2 = ot && std::string_view(ot->GetName()) == "FnVector2";
		if (vec2 && name == "LengthSq") {
			const float* a = static_cast<const float*>(gen->GetObject());
			gen->SetReturnFloat(a ? a[0] * a[0] + a[1] * a[1] : 0.0f);
			return DispatchResult::Handled;
		}
		if (vec2 && name == "IncDistance") {
			// (distance, radians) — verified from GetDokataCurveTrack's atan2 call.
			float* a = static_cast<float*>(gen->GetObject());
			if (a) {
				const float dist = gen->GetArgFloat(0);
				const float rad = gen->GetArgFloat(1);
				a[0] += dist * std::cos(rad);
				a[1] += dist * std::sin(rad);
			}
			return DispatchResult::Handled;
		}
		if (vec2 && name == "opEquals") {
			const float* a = static_cast<const float*>(gen->GetObject());
			const float* b = static_cast<const float*>(gen->GetArgAddress(0));
			gen->SetReturnByte(a && b && a[0] == b[0] && a[1] == b[1] ? 1 : 0);
			return DispatchResult::Handled;
		}
	}
	if (name == "GetRandom") {
		static std::mt19937 rng{0x5eed1234u};
		if (gen->GetArgCount() >= 2) {
			int lo = static_cast<int>(gen->GetArgDWord(0));
			int hi = static_cast<int>(gen->GetArgDWord(1));
			if (lo > hi)
				std::swap(lo, hi);
			std::uniform_int_distribution<int> dist(lo, hi > lo ? hi - 1 : lo);
			gen->SetReturnDWord(static_cast<asDWORD>(dist(rng)));
		} else {
			gen->SetReturnFloat(std::uniform_real_distribution<float>(0.0f, 1.0f)(rng));
		}
		return DispatchResult::Handled;
	}
	if (name == "DbPrint") {
		gen->SetReturnDWord(0);
		return DispatchResult::Handled;
	}
	if (name == "DbInt" || name == "DbAddFriendshipAllChara" || name == "DbClearNotify" || name == "HideResource" ||
		name == "ShowResource" || name == "AutoSave" || name == "SetTimerColorTableIndex" ||
		name == "SetCharaGradientMapCommon")
		return DispatchResult::Handled;
	if (name == "DbIsDebugStage" || name == "DbIsKeyTrigger" || name == "IsFriendshipTalkReplay" ||
		name == "IsVeryHard" || name == "IsPlayStation" || name == "IsXBX" || name == "IsOfflineFirstUnit") {
		gen->SetReturnByte(0);
		return DispatchResult::Handled;
	}
	if (name == "IsJapanese") {
		// The data set is the JP build.
		gen->SetReturnByte(1);
		return DispatchResult::Handled;
	}
	if (name == "GetTextLanguage" || name == "GetVoiceLanguage") {
		gen->SetReturnDWord(0);
		return DispatchResult::Handled;
	}
	if (name == "SetAllSymbolColorA") {
		push_ui("symbol_color_a", {}, I(0), I(1), I(2), nullptr, 0.0f, false);
		return DispatchResult::Handled;
	}
	if (name == "SetColor") {
		float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		args.color(1, color);
		RuntimeUi& entry = push_ui("set_color", S(0), I(2), 0, 0, nullptr, 0.0f, false);
		entry.ints = {color_byte(color[0]), color_byte(color[1]), color_byte(color[2]), color_byte(color[3])};
		return DispatchResult::Handled;
	}
	if (name == "SetDSPSnapshot") {
		// No DSP snapshot support; return an empty snapshot name.
		if (auto* out = static_cast<std::string*>(gen->GetAddressOfReturnLocation()))
			new (out) std::string();
		return DispatchResult::Handled;
	}

	return DispatchResult::Unhandled;
}

} // namespace asb
