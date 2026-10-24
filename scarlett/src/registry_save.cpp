#include "native_args.hpp"
#include "registry.hpp"

#include <angelscript.h>

#include <string>
#include <string_view>

namespace asb {

DispatchResult Registry::dispatch_save(std::string_view name, NativeArgs& args) {
	asIScriptGeneric* gen = args.generic();
	auto S = [&](int i) { return args.str(i); };
	auto I = [&](int i) { return args.i32(i); };
	auto V = [&](int i, float* out, int count) {
		if (count == 2)
			args.vec2(i, out);
		else
			args.vec3(i, out);
	};

	// Script-accumulated save/party state: the mutators populate the stores; the
	// getters read them back.
	if (name == "AddCharaSaveData") {
		save().party.insert(S(0));
		return DispatchResult::Handled;
	}
	if (name == "DeleteCharaSaveData" || name == "DeleteMobCharaSaveData") {
		save().party.erase(S(0));
		return DispatchResult::Handled;
	}
	if (name == "IsExistCharaSaveData") {
		gen->SetReturnByte(save().party.count(S(0)) ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "SetSlotFlag") {
		save().slots[I(0)] = I(1);
		return DispatchResult::Handled;
	}
	if (name == "GetSlotFlag") {
		auto it = save().slots.find(I(0));
		gen->SetReturnDWord(it == save().slots.end() ? 0u : static_cast<asDWORD>(it->second));
		return DispatchResult::Handled;
	}
	if (name == "GetSlotBitFlag") {
		auto it = save().slots.find(I(0));
		gen->SetReturnByte((it != save().slots.end() && it->second) ? 1 : 0);
		return DispatchResult::Handled;
	}

	if (name == "SetSlotBitFlag") {
		save().slots[I(0)] = I(1) ? 1 : 0;
		return DispatchResult::Handled;
	}
	if (name == "GetStageBitFlag") {
		gen->SetReturnByte(save().stage_bits.count(I(0)) ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "ClassChangeChara") {
		save().class_changed.insert(S(0));
		return DispatchResult::Handled;
	}
	if (name == "IsClassChangeChara") {
		gen->SetReturnByte(save().class_changed.count(S(0)) ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "SetDisableChara") {
		const std::string n = S(0);
		if (I(1))
			save().disabled.insert(n);
		else
			save().disabled.erase(n);
		return DispatchResult::Handled;
	}
	if (name == "IsDisableChara") {
		gen->SetReturnByte(save().disabled.count(S(0)) ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "AddUnitSaveData") {
		save().units.insert(I(0));
		return DispatchResult::Handled;
	}
	{
		const bool complete = name.find("Complete") != std::string_view::npos;
		if (name == "QuestOpen" || name == "QuestComplete") {
			save().quests[I(0)] |= complete ? 3 : 1;
			return DispatchResult::Handled;
		}
		if (name == "IsQuestOpened" || name == "IsQuestCompleted") {
			auto it = save().quests.find(I(0));
			const int bit = complete ? 2 : 1;
			gen->SetReturnByte((it != save().quests.end() && (it->second & bit)) ? 1 : 0);
			return DispatchResult::Handled;
		}
	}
	if (name == "SetUnitDir") {
		_unit_dir[S(0)] = I(1);
		return DispatchResult::Handled;
	}
	if (name == "GetUnitDegree") {
		auto it = _unit_dir.find(S(0));
		gen->SetReturnDWord(it == _unit_dir.end() ? 0u : static_cast<asDWORD>(it->second));
		return DispatchResult::Handled;
	}

	// Save/party getters with no script-side mutator. There is no save file and
	// none is required: the script-facing state API is entirely in-memory (owned
	// by this shim), so the plan is to invent our own save format and seed these
	// from it. TODO(save): define that format and wire it up; until then, report
	// defaults.
	// TODO(save): save-derived; defaults for now.
	if (name == "GetCharaLv" || name == "GetCharaAverageLv" || name == "GetArmyType") {
		gen->SetReturnDWord(0);
		return DispatchResult::Handled;
	}
	if (name == "GetGold") {
		gen->SetReturnDWord(static_cast<asDWORD>(save().gold));
		return DispatchResult::Handled;
	}
	if (name == "GetFameRank") {
		gen->SetReturnDWord(static_cast<asDWORD>(save().fame));
		return DispatchResult::Handled;
	}
	if (name == "GetItemNum") {
		auto it = save().items.find(S(0));
		gen->SetReturnDWord(it == save().items.end() ? 0u : static_cast<asDWORD>(it->second));
		return DispatchResult::Handled;
	}
	// TODO(save): IsLeader is save-derived.
	if (name == "IsLeader" || name == "StIsPartnerSelectCharaDefName" || name == "IsEmptyPlayerCharaList") {
		gen->SetReturnByte((name == "IsEmptyPlayerCharaList" && save().party.empty()) ? 1 : 0);
		return DispatchResult::Handled;
	}

	// Game-state stores.
	if (name == "AddGold") {
		save().gold += I(0);
		return DispatchResult::Handled;
	}
	if (name == "SetGold") {
		save().gold = I(0);
		return DispatchResult::Handled;
	}
	if (name == "AddFame") {
		save().fame += I(0);
		return DispatchResult::Handled;
	}
	if (name == "AddSlotBitFlag") {
		save().slots[I(0)] = 1;
		return DispatchResult::Handled;
	}
	if (name == "DelSlotBitFlag") {
		save().slots[I(0)] = 0;
		return DispatchResult::Handled;
	}
	if (name == "AddStageBitFlag") {
		save().stage_bits.insert(I(0));
		return DispatchResult::Handled;
	}
	if (name == "SetStageBitFlag") {
		if (I(1))
			save().stage_bits.insert(I(0));
		else
			save().stage_bits.erase(I(0));
		return DispatchResult::Handled;
	}
	if (name == "SetStageFlag") {
		save().stage_flags[I(0)] = I(1);
		return DispatchResult::Handled;
	}
	if (name == "AddStageFlag") {
		save().stage_flags[I(0)] = I(1);
		return DispatchResult::Handled;
	}
	if (name == "GetStageFlag") {
		auto it = save().stage_flags.find(I(0));
		gen->SetReturnDWord(it == save().stage_flags.end() ? 0u : static_cast<asDWORD>(it->second));
		return DispatchResult::Handled;
	}
	if (name == "GetClearedStageNum") {
		// TODO(save): save-derived cleared-stage count; proxy = stage bit count.
		gen->SetReturnDWord(static_cast<asDWORD>(save().stage_bits.size()));
		return DispatchResult::Handled;
	}
	if (name == "SetGlobalFlag") {
		save().global_flags[I(0)] = I(1);
		return DispatchResult::Handled;
	}
	if (name == "GetGlobalFlag") {
		auto it = save().global_flags.find(I(0));
		gen->SetReturnDWord(it == save().global_flags.end() ? 0u : static_cast<asDWORD>(it->second));
		return DispatchResult::Handled;
	}
	if (name == "AddGlobalBitFlag") {
		save().global_flags[I(0)] = 1;
		return DispatchResult::Handled;
	}
	if (name == "GetGlobalBitFlag") {
		gen->SetReturnByte(save().global_flags.count(I(0)) && save().global_flags[I(0)] ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "QuestPreOpen") {
		save().quests[I(0)] |= 1;
		save().quest_preopened.insert(I(0));
		return DispatchResult::Handled;
	}
	if (name == "IsQuestPreOpened") {
		gen->SetReturnByte(save().quest_preopened.count(I(0)) ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "QuestFailed") {
		save().quests[I(0)] |= 8;
		return DispatchResult::Handled;
	}
	if (name == "SetQuestPin") {
		save().quest_pinned.insert(I(0));
		return DispatchResult::Handled;
	}
	if (name == "IsExistPinnedQuest") {
		gen->SetReturnByte(save().quest_pinned.empty() ? 0 : 1);
		return DispatchResult::Handled;
	}
	if (name == "RegisterFacilityToWM") {
		save().facilities.insert(S(0));
		return DispatchResult::Handled;
	}
	if (name == "IsFacilityRegisteredToWM") {
		gen->SetReturnByte(save().facilities.count(S(0)) ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "AddFriendshipPoint") {
		save().friendship[S(0) + "|" + S(1)] += I(2);
		return DispatchResult::Handled;
	}
	// TODO(save): the level formula (points / 100) is invented.
	if (name == "GetFriendshipLV") {
		auto it = save().friendship.find(S(0) + "|" + S(1));
		gen->SetReturnDWord(it == save().friendship.end() ? 0u : static_cast<asDWORD>(it->second / 100));
		return DispatchResult::Handled;
	}
	if (name == "RemoveEquipItem") {
		auto it = save().items.find(S(1));
		if (it != save().items.end() && it->second > 0)
			--it->second;
		return DispatchResult::Handled;
	}
	if (name == "IsEmptyPlayerUnitList") {
		gen->SetReturnByte(save().units.empty() ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "IsTreasureOpenedAny") {
		gen->SetReturnByte(save().treasures.empty() ? 0 : 1);
		return DispatchResult::Handled;
	}
	if (name == "SetQuestTargetText" || name == "AddQuestTargetText") {
		push_field("quest_text", {}, S(1) + " " + S(2), I(0), I(3), 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "UpdateQuestText") {
		push_field("quest_update", {}, {}, I(0), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "SetQuestMarker" || name == "AddQuestMarker") {
		float p[2] = {0.0f, 0.0f};
		V(2, p, 2);
		push_field("quest_marker", S(0), {}, I(1), 0, 0, 0.0f, p, false);
		return DispatchResult::Handled;
	}
	if (name == "DeleteQuestMarker") {
		push_field("quest_marker_del", S(0), {}, 0, 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "ClearQuestMarker") {
		push_field("quest_marker_clear", {}, {}, I(0), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "SetQuestTargetStage" || name == "AddQuestTargetStage") {
		push_field("quest_stage", S(1), {}, I(0), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "SetQuestTargetTalkTo" || name == "AddQuestTargetTalkTo") {
		push_field("quest_talk", S(1), {}, I(0), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "SetQuestTargetBattle") {
		push_field("quest_battle", S(1), {}, I(0), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "SetQuestTargetDestination") {
		push_field("quest_dest", S(1), {}, I(0), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}
	if (name == "ClearQuestTarget") {
		push_field("quest_target_clear", {}, {}, I(0), 0, 0, 0.0f, nullptr, false);
		return DispatchResult::Handled;
	}

	if (name == "StGetStageStartParam") {
		gen->SetReturnDWord(static_cast<asDWORD>(_stage_start_param));
		return DispatchResult::Handled;
	}

	if (name == "GetOfflineRank") {
		// TODO(save): rank is save-derived; default.
		gen->SetReturnDWord(0);
		return DispatchResult::Handled;
	}
	if (name == "IsSecondRanking") {
		// TODO(save): save-derived; default.
		gen->SetReturnByte(0);
		return DispatchResult::Handled;
	}

	if (name == "DrEnableBalconyFlagBRU") {
		save().balcony_bru = true;
		return DispatchResult::Handled;
	}
	if (name == "DrEnableBalconyFlagSCR") {
		save().balcony_scr = true;
		return DispatchResult::Handled;
	}
	if (name == "DrGetBalconyFlagBRU") {
		gen->SetReturnByte(save().balcony_bru ? 1 : 0);
		return DispatchResult::Handled;
	}
	if (name == "DrGetBalconyFlagSCR") {
		gen->SetReturnByte(save().balcony_scr ? 1 : 0);
		return DispatchResult::Handled;
	}
	return DispatchResult::Unhandled;
}

} // namespace asb
