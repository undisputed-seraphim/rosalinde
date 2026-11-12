#include "native_args.hpp"
#include "registry.hpp"

#include <angelscript.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
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

long text_char_count(std::string_view text) {
	long chars = 0;
	for (size_t i = 0; i < text.size();) {
		const unsigned char c = static_cast<unsigned char>(text[i]);
		if (c == '@') {
			++i;
			while (i < text.size() &&
				   (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_'))
				++i;
			if (i < text.size() && text[i] == '(') {
				int depth = 1;
				for (++i; i < text.size() && depth > 0; ++i) {
					if (text[i] == '(')
						++depth;
					else if (text[i] == ')')
						--depth;
				}
			}
			continue;
		}
		if (c == '\r' || c == '\n' || c == '\t') {
			++i;
			continue;
		}
		if ((c & 0xC0) != 0x80)
			++chars;
		++i;
	}
	return chars;
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

	auto text_wait = [&](RuntimeText::Kind kind, const std::string& text) -> long {
		const long chars = text_char_count(text);
		if (kind == RuntimeText::Kind::Sprite)
			return std::max(1L, chars * _message_timing.sprite_per_char);
		return std::max<long>(_message_timing.voice_off_min, chars * _message_timing.voice_off_per_char);
	};

	auto emit =
		[&](RuntimeText::Kind kind,
			std::string speaker,
			std::string text,
			std::vector<int> ids,
			int balloon_arg,
			int pos_arg,
			long at_frame) -> RuntimeText& {
		RuntimeText entry;
		entry.frame = at_frame;
		entry.kind = kind;
		entry.speaker = std::move(speaker);
		entry.text = std::move(text);
		entry.ids = std::move(ids);
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
		entry.end = at_frame + text_wait(kind, entry.text);
		if (text_trace()) {
			std::fprintf(
				stderr,
				"[text f=%ld] kind=%d speaker=\"%s\" disp=%d name=%d dir=%d flags=%x pos=(%.1f,%.1f): %s\n",
				at_frame,
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

	auto parse_ids = [&](int ids_arg) -> std::vector<int> {
		std::vector<int> ids;
		if (ids_arg < 0)
			return ids;
		auto* array = static_cast<CScriptArray*>(gen->GetArgObject(ids_arg));
		if (!array)
			return ids;
		const asUINT count = array->GetSize();
		const asUINT probe = std::min<asUINT>(count, 64);
		bool all_zero = true;
		for (asUINT i = 0; i < probe; ++i) {
			if (*static_cast<const int*>(array->At(i)) != 0) {
				all_zero = false;
				break;
			}
		}
		if (count > 0 && all_zero) {
			// The game's `$fact(int)` message helper builds a zero-filled
			// array whose length is the local message id.
			const int id = static_cast<int>(count);
			ids.push_back(id < 0x10000 ? id + 0x10000 : id);
		} else if (!all_zero) {
			for (asUINT i = 0; i < probe; ++i) {
				const int value = *static_cast<const int*>(array->At(i));
				ids.push_back(value >= 0 && value < 0x10000 ? value + 0x10000 : value);
			}
		}
		return ids;
	};

	auto play_voice_id = [this](int id, long at_frame) -> long {
		if (!_play_voice || id < 0)
			return 0;
		return std::max<long>(0, _play_voice(id, at_frame));
	};

	auto page_end = [&](RuntimeText& entry, long voice_frames) {
		if (voice_frames <= 0)
			return;
		const long text_frames = std::max(1L, entry.end - entry.frame);
		entry.end = entry.frame + std::max<long>(_message_timing.voice_wait_min, voice_frames) + text_frames;
	};

	auto message_page_text = [this](int id) -> std::string {
		if (!_message_text || id < 0)
			return {};
		return _message_text(id);
	};

	// A message-id array holds one page per id. The game pops the pages in
	// order, so each page gets its own voice, voice wait and text wait.
	auto emit_message = [&](RuntimeText::Kind kind,
							std::string speaker,
							std::string text,
							int ids_arg,
							int balloon_arg,
							int pos_arg) {
		std::vector<int> ids = parse_ids(ids_arg);
		if (ids.size() <= 1 || !_message_text) {
			RuntimeText& entry =
				emit(kind, std::move(speaker), std::move(text), std::move(ids), balloon_arg, pos_arg, frame);
			page_end(entry, play_voice_id(entry.ids.empty() ? -1 : entry.ids.front(), frame));
			return;
		}

		long page_start = frame;
		for (size_t i = 0; i < ids.size(); ++i) {
			const int id = ids[i];
			std::string page = message_page_text(id);
			if (page.empty()) {
				if (i == 0)
					page = text;
				else
					spdlog::debug("text: no page text for message id {}", id);
			}
			RuntimeText& entry =
				emit(kind, speaker, std::move(page), std::vector<int>{id}, balloon_arg, pos_arg, page_start);
			page_end(entry, play_voice_id(id, page_start));
			page_start = entry.end;
		}
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
		emit_message(
			screen ? RuntimeText::Kind::Screen : RuntimeText::Kind::Msg, args.str(0), args.str(1), 2, 3, 4);
		block_text();
		return DispatchResult::Handled;
	}
	if (name == "MsgAsync" || name == "MsgScreenAsync") {
		emit_message(
			name == "MsgScreenAsync" ? RuntimeText::Kind::Screen : RuntimeText::Kind::Msg,
			args.str(0),
			args.str(1),
			2,
			3,
			4);
		return DispatchResult::Handled;
	}
	if (name == "SelectMsg" || name == "SelectMsgScreen") {
		emit_message(RuntimeText::Kind::Select, args.str(0), args.str(1), 2, 3, 4);
		_select_result = 0; // deterministic default: first option
		block_text();
		return DispatchResult::Handled;
	}
	if (name == "MsgStageVoice") {
		emit(RuntimeText::Kind::Msg, args.str(0), {}, {}, 2, 3, frame);
		block_text();
		return DispatchResult::Handled;
	}
	if (name == "Dialog" || name == "YNDialog") {
		emit(name == "YNDialog" ? RuntimeText::Kind::YesNo : RuntimeText::Kind::Dialog,
			 {},
			 args.str(0),
			 {},
			 -1,
			 -1,
			 frame);
		if (name == "YNDialog")
			_select_result = 0;
		block_text();
		return DispatchResult::Handled;
	}
	if (name == "CommonMsg") {
		emit(RuntimeText::Kind::Msg, {}, args.str(0), {}, -1, -1, frame);
		block_text();
		return DispatchResult::Handled;
	}
	if (name == "StShowTutorialMsg") {
		emit(RuntimeText::Kind::Msg, {}, args.str(1), {}, -1, -1, frame);
		return DispatchResult::Handled;
	}
	if (name == "Message") {
		RuntimeText& entry = emit(RuntimeText::Kind::Message, {}, args.str(0), {}, -1, -1, frame);
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
		RuntimeText& entry = emit(RuntimeText::Kind::Sprite, args.str(0), args.str(1), {}, -1, 3, frame);
		if (gen->GetArgCount() > 9) {
			const int period = args.i32(9);
			if (period >= 0)
				entry.end = frame + period;
		}
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
