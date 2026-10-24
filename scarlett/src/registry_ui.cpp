#include "native_args.hpp"
#include "registry.hpp"

#include <angelscript.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace asb {

RuntimeUi&
Registry::push_ui(std::string kind, std::string text, int id, int i0, int i1, const float* pos, float rate, bool flag) {
	RuntimeUi entry;
	entry.frame = frame;
	entry.kind = std::move(kind);
	entry.text = std::move(text);
	entry.id = id;
	entry.i0 = i0;
	entry.i1 = i1;
	if (pos) {
		entry.pos[0] = pos[0];
		entry.pos[1] = pos[1];
	}
	entry.rate = rate;
	entry.flag = flag;
	_timeline.ui.push_back(std::move(entry));
	return _timeline.ui.back();
}

DispatchResult Registry::dispatch_ui(std::string_view name, NativeArgs& args) {
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

	if (name == "DrIsArchiveMode") {
		gen->SetReturnByte(0);
		return DispatchResult::Handled;
	}
	if (name == "OpenArchive" || name == "CloseArchive") {
		RuntimeUi& u =
			push_ui(name == "OpenArchive" ? "archive_open" : "archive_close", S(0), 0, 0, 0, nullptr, 0.0f, false);
		u.ints = args.ints(1);
		return DispatchResult::Handled;
	}
	if (name == "CreateFacilityName") {
		float p[2] = {0.0f, 0.0f};
		V(2, p, 2);
		push_ui("facility_name", S(0), I(1), I(3), 0, p, 0.0f, I(4) != 0);
		return DispatchResult::Handled;
	}
	if (name == "DeleteFacilityName") {
		push_ui("facility_name_delete", {}, 0, I(0), 0, nullptr, 0.0f, false);
		return DispatchResult::Handled;
	}
	if (name == "ShowFieldInfo") {
		push_ui("field_info", {}, 0, 0, 0, nullptr, 0.0f, false);
		return DispatchResult::Handled;
	}
	if (name == "ShowFacilityInfo") {
		push_ui("facility_info", S(1), I(0), I(2), I(3), nullptr, 0.0f, false);
		const long wait = std::max(0, I(3));
		if (wait > 0) {
			pending_wait = wait;
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return DispatchResult::Handled;
	}
	if (name == "ShowExStageTitle") {
		push_ui("ex_stage_title", {}, I(0), I(1), 0, nullptr, 0.0f, false);
		return DispatchResult::Handled;
	}
	if (name == "SetClassPedia_ByDefName") {
		push_ui("class_pedia", S(0), 0, 0, 0, nullptr, 0.0f, false);
		return DispatchResult::Handled;
	}
	if (name == "OpenClassPedia") {
		_class_pedia_open = I(0) != 0;
		push_ui("class_pedia_open", {}, 0, 0, 0, nullptr, 0.0f, _class_pedia_open);
		return DispatchResult::Handled;
	}
	if (name == "SetAllOpenClassPediaBG") {
		_all_class_pedia_bg = true;
		push_ui("class_pedia_bg", {}, 0, 0, 0, nullptr, 0.0f, true);
		return DispatchResult::Handled;
	}
	if (name == "SetAddClassPediaFlag") {
		_add_class_pedia_flag = I(0) != 0;
		push_ui("class_pedia_flag", {}, 0, 0, 0, nullptr, 0.0f, _add_class_pedia_flag);
		return DispatchResult::Handled;
	}
	if (name == "CreateStaffRoll") {
		// TODO(staff-roll): scroll timing not modelled.
		push_ui("staff_roll", S(0), I(1), 0, 0, nullptr, 0.0f, false);
		return DispatchResult::Handled;
	}
	if (name == "SetScrollStaffRollFlag") {
		_staff_roll_scroll = I(0) != 0;
		_staff_roll_speed = F(1);
		push_ui("staff_roll_scroll", {}, 0, 0, 0, nullptr, _staff_roll_speed, _staff_roll_scroll);
		return DispatchResult::Handled;
	}
	if (name == "StOpenPartnerSelect") {
		_partner_select_open = true;
		push_ui("partner_select_open", {}, 0, 0, 0, nullptr, 0.0f, true);
		return DispatchResult::Handled;
	}
	if (name == "StIsPartnerSelect") {
		gen->SetReturnByte(_partner_select_open ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "StClearPartnerSelect") {
		_partner_select_open = false;
		push_ui("partner_select_close", {}, 0, 0, 0, nullptr, 0.0f, false);
		return DispatchResult::Handled;
	}
	if (name == "StIsPartnerSelectRequired" || name == "StIsEnableOpenPartnerSelect") {
		// Returning false skips the partner-select UI. This is a headless-playback
		// choice, not verified engine behaviour; revisit when fine-tuning.
		gen->SetReturnByte(0);
		return DispatchResult::Handled;
	}
	if (name == "OpenUpgradeEquipUI") {
		push_ui("upgrade_equip", {}, 0, 0, 0, nullptr, 0.0f, false);
		return DispatchResult::Handled;
	}
	if (name == "OpenUnitInfoUI") {
		push_ui("unit_info", {}, 0, 0, 0, nullptr, 0.0f, false);
		return DispatchResult::Handled;
	}
	if (name == "OpenDifficultyChoiceUI") {
		push_ui("difficulty", {}, 0, 0, 0, nullptr, 0.0f, false);
		return DispatchResult::Handled;
	}
	if (name == "OpenShopUI") {
		push_ui("shop", {}, I(0), 0, 0, nullptr, 0.0f, false);
		return DispatchResult::Handled;
	}
	if (name == "SetDisableBackLog") {
		_disable_backlog = I(0) != 0;
		push_ui("disable_backlog", {}, 0, 0, 0, nullptr, 0.0f, _disable_backlog);
		return DispatchResult::Handled;
	}
	if (name == "SetDisableOption") {
		_disable_option = I(0) != 0;
		push_ui("disable_option", {}, 0, 0, 0, nullptr, 0.0f, _disable_option);
		return DispatchResult::Handled;
	}
	if (name == "EnableTitleLibrary") {
		_title_library = true;
		push_ui("title_library", {}, 0, 0, 0, nullptr, 0.0f, true);
		return DispatchResult::Handled;
	}
	if (name == "UnlockTrophy") {
		push_ui("trophy", {}, I(0), I(1), 0, nullptr, 0.0f, false);
		const long wait = std::max(0, I(1));
		if (wait > 0) {
			pending_wait = wait;
			if (asIScriptContext* ctx = asGetActiveContext())
				ctx->Suspend();
		}
		return DispatchResult::Handled;
	}
	if (name == "UnlockTrophyAllDelivery") {
		push_ui("trophy_all", {}, 0, 0, 0, nullptr, 0.0f, false);
		return DispatchResult::Handled;
	}

	return DispatchResult::Unhandled;
}

} // namespace asb
