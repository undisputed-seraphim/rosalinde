#include "native_args.hpp"
#include "registry.hpp"

#include <angelscript.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <string_view>

namespace asb {

namespace {

bool text_trace() {
	static const bool enabled = std::getenv("ASB_TEXT") != nullptr;
	return enabled;
}

long text_duration(const std::string& text) {
	size_t chars = 0;
	for (unsigned char c : text)
		if ((c & 0xC0) != 0x80)
			++chars;
	return 30 + static_cast<long>(chars); // TODO(timing): estimate
}

} // namespace

DispatchResult Registry::dispatch_text(std::string_view name, NativeArgs& args) {
	asIScriptGeneric* gen = args.generic();
	asIScriptFunction* fn = gen->GetFunction();

	// Scripts pass the message text literally, so record it (with speaker,
	// balloon style and position) and model the UcMsgBalloonParam /
	// UcSpriteTextParam objects the setters mutate. Pixel rendering is deferred;
	// `ASB_TEXT` logs the text.
	asITypeInfo* object_type = fn->GetObjectType();
	const std::string object_name = (object_type && object_type->GetName()) ? object_type->GetName() : "";
	void* self = gen->GetObject();

	auto balloon_of = [&](int arg) -> BalloonParam* {
		void* pointer = gen->GetArgObject(arg);
		if (!pointer)
			pointer = gen->GetArgAddress(arg);
		auto it = _balloons.find(pointer);
		return it == _balloons.end() ? nullptr : &it->second;
	};

	auto emit =
		[&](RuntimeText::Kind kind, std::string speaker, std::string text, int ids_arg, int balloon_arg, int pos_arg)
		-> RuntimeText& {
		RuntimeText entry;
		entry.frame = frame;
		entry.kind = kind;
		entry.speaker = std::move(speaker);
		entry.text = std::move(text);
		if (ids_arg >= 0) {
			// The Msg ID array is engine-internal (voice ids); the large ones are
			// uninitialised in our model, so only record compact arrays.
			if (auto* array = static_cast<CScriptArray*>(gen->GetArgObject(ids_arg)))
				if (array->GetSize() <= 64)
					entry.ids = args.ints(ids_arg);
		}
		if (balloon_arg >= 0) {
			if (BalloonParam* balloon = balloon_of(balloon_arg)) {
				entry.disp_type = balloon->disp_type;
				entry.disp_name = balloon->disp_name;
				entry.face_dir_x = balloon->face_dir_x;
				entry.flags = balloon->flags;
			}
		}
		if (pos_arg >= 0)
			args.vec2(pos_arg, entry.pos);
		entry.end = frame + text_duration(entry.text);
		if (text_trace()) {
			std::fprintf(
				stderr,
				"[text f=%ld] kind=%d speaker=\"%s\" disp=%d name=%d dir=%d flags=%x pos=(%.1f,%.1f): %s\n",
				frame,
				static_cast<int>(kind),
				entry.speaker.c_str(),
				entry.disp_type,
				entry.disp_name,
				entry.face_dir_x,
				entry.flags,
				entry.pos[0],
				entry.pos[1],
				entry.text.c_str());
		}
		_timeline.texts.push_back(std::move(entry));
		return _timeline.texts.back();
	};

	// Msg/Dialog/Select block until the message has been read; park the script
	// for the estimated display time and let the scheduler resume it.
	auto block_text = [&]() -> bool {
		if (_timeline.texts.empty())
			return true;
		const long end = _timeline.texts.back().end;
		const long duration = std::max(1L, end - frame);
		pending_wait = duration;
		spdlog::debug("wait: text frame={} end={}", frame, end);
		if (asIScriptContext* context = asGetActiveContext())
			context->Suspend();
		return true;
	};

	if (object_name == "UcMsgBalloonParam") {
		if (name == "SetDispType")
			_balloons[self].disp_type = args.i32(0);
		else if (name == "SetDispName")
			_balloons[self].disp_name = args.i32(0);
		else if (name == "SetFaceDirX")
			_balloons[self].face_dir_x = args.i32(0);
		else if (name == "AddBalloonFlag")
			_balloons[self].flags |= static_cast<unsigned>(args.i32(0));
		else if (name == "DelBalloonFlag")
			_balloons[self].flags &= ~static_cast<unsigned>(args.i32(0));
		else if (name == "opAssign") {
			if (BalloonParam* source = balloon_of(0))
				_balloons[self] = *source;
			gen->SetReturnAddress(self);
		}
		return DispatchResult::Handled;
	}
	if (object_name == "UcSpriteTextParam") {
		SpriteTextParam& param = _sprite_texts[self];
		if (name == "SetAlign")
			param.align = args.i32(0);
		else if (name == "SetFontType")
			param.font_type = args.i32(0);
		else if (name == "SetOutline")
			param.outline = args.i32(0);
		else if (name == "SetColorR")
			param.color[0] = args.i32(0);
		else if (name == "SetColorG")
			param.color[1] = args.i32(0);
		else if (name == "SetColorB")
			param.color[2] = args.i32(0);
		else if (name == "SetScale")
			param.scale = args.f32(0);
		else if (name == "SetBaseLine")
			param.base_line = args.f32(0);
		else if (name == "SetForceInstant")
			param.force_instant = args.i32(0) != 0;
		else if (name == "opAssign") {
			void* source = gen->GetArgObject(0);
			if (!source)
				source = gen->GetArgAddress(0);
			auto it = _sprite_texts.find(source);
			if (it != _sprite_texts.end())
				param = it->second;
			gen->SetReturnAddress(self);
		}
		return DispatchResult::Handled;
	}

	if (name == "Msg" || name == "MsgScreen") {
		const bool screen = name == "MsgScreen";
		emit(screen ? RuntimeText::Kind::Screen : RuntimeText::Kind::Msg, args.str(0), args.str(1), 2, 3, 4);
		block_text();
		return DispatchResult::Handled;
	}
	if (name == "MsgAsync" || name == "MsgScreenAsync") {
		emit(
			name == "MsgScreenAsync" ? RuntimeText::Kind::Screen : RuntimeText::Kind::Msg,
			args.str(0),
			args.str(1),
			2,
			3,
			4);
		return DispatchResult::Handled;
	}
	if (name == "SelectMsg" || name == "SelectMsgScreen") {
		emit(RuntimeText::Kind::Select, args.str(0), args.str(1), 2, 3, 4);
		_select_result = 0; // deterministic default: first option
		block_text();
		return DispatchResult::Handled;
	}
	if (name == "MsgStageVoice") {
		emit(RuntimeText::Kind::Msg, args.str(0), {}, -1, 2, 3);
		block_text();
		return DispatchResult::Handled;
	}
	if (name == "Dialog" || name == "YNDialog") {
		emit(name == "YNDialog" ? RuntimeText::Kind::YesNo : RuntimeText::Kind::Dialog, {}, args.str(0), -1, -1, -1);
		if (name == "YNDialog")
			_select_result = 0;
		block_text();
		return DispatchResult::Handled;
	}
	if (name == "CommonMsg") {
		emit(RuntimeText::Kind::Msg, {}, args.str(0), -1, -1, -1);
		block_text();
		return DispatchResult::Handled;
	}
	if (name == "StShowTutorialMsg") {
		emit(RuntimeText::Kind::Msg, {}, args.str(1), -1, -1, -1);
		return DispatchResult::Handled;
	}
	if (name == "Message") {
		RuntimeText& entry = emit(RuntimeText::Kind::Message, {}, args.str(0), -1, -1, -1);
		entry.end = 0; // banner is open until CloseMessage()
		if (args.i32(1) >= 0)
			entry.ids.push_back(args.i32(1));
		return DispatchResult::Handled;
	}
	if (name == "CloseMessage") {
		for (auto it = _timeline.texts.rbegin(); it != _timeline.texts.rend(); ++it) {
			if (it->kind == RuntimeText::Kind::Message && it->end == 0) {
				it->end = frame;
				break;
			}
		}
		return DispatchResult::Handled;
	}
	if (name == "MsgFinishWait") {
		// Async messages finish here: park until the last one's display ends.
		if (!_timeline.texts.empty() && _timeline.texts.back().end > frame) {
			pending_wait = _timeline.texts.back().end - frame;
			if (asIScriptContext* context = asGetActiveContext())
				context->Suspend();
		}
		return DispatchResult::Handled;
	}
	if (name == "CloseMsg") {
		if (!_timeline.texts.empty())
			_timeline.texts.back().end = frame;
		return DispatchResult::Handled;
	}
	if (name == "StCloseTutorialMsg" || name == "DrAlwaysUpdateMsgPos")
		return DispatchResult::Handled;
	if (name == "GetResultSelectMsg" || name == "GetYNDialogResult") {
		gen->SetReturnDWord(static_cast<asDWORD>(_select_result));
		return DispatchResult::Handled;
	}
	if (name == "CreateSpriteText") {
		emit(RuntimeText::Kind::Msg, args.str(0), args.str(1), -1, -1, 3);
		gen->SetReturnDWord(static_cast<asDWORD>(_sprite_text_handle++));
		return DispatchResult::Handled;
	}
	if (name == "SpriteTextFinishWait" || name == "MulSpriteTextOffsetY")
		return DispatchResult::Handled;
	if (name == "SetMsgFaceAnimation" || name == "SetMsgFaceLayerMask" || name == "ShowAddCharaDialog")
		return DispatchResult::Handled;

	return DispatchResult::Unhandled;
}

} // namespace asb
