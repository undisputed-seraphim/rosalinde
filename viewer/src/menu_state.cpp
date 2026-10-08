#include "menu_state.hpp"

MenuState::MenuState(const std::vector<std::string>& class_names, const std::vector<std::string>& bg_names)
	: _class_names(class_names)
	, _bg_names(bg_names) {}

bool MenuState::draw(MenuSelection& out) {
	bool started = false;

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

	ImGui::Begin("Rosalinde", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	ImGui::Text("Character Select");
	ImGui::Separator();

	if (ImGui::BeginCombo("Left", _class_names[_left_idx].c_str())) {
		for (int i = 0; i < (int)_class_names.size(); ++i) {
			if (ImGui::Selectable(_class_names[i].c_str(), i == _left_idx))
				_left_idx = i;
		}
		ImGui::EndCombo();
	}

	if (ImGui::BeginCombo("Right", _class_names[_right_idx].c_str())) {
		for (int i = 0; i < (int)_class_names.size(); ++i) {
			if (ImGui::Selectable(_class_names[i].c_str(), i == _right_idx))
				_right_idx = i;
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();
	ImGui::Text("Background");
	if (ImGui::BeginCombo("Stage##bg", _bg_names[_bg_idx].c_str())) {
		for (int i = 0; i < (int)_bg_names.size(); ++i) {
			if (ImGui::Selectable(_bg_names[i].c_str(), i == _bg_idx))
				_bg_idx = i;
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();
	ImGui::Spacing();

	if (ImGui::Button("Fight!", ImVec2(120, 40))) {
		out.left_class = _class_names[_left_idx];
		out.right_class = _class_names[_right_idx];
		out.background = _bg_names[_bg_idx];
		started = true;
	}

	ImGui::End();
	return started;
}
