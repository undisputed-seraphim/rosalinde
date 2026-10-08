#pragma once

#include <imgui.h>
#include <string>
#include <vector>

struct MenuSelection {
	std::string left_class;
	std::string right_class;
	std::string background;
};

struct MenuState {
	MenuState(const std::vector<std::string>& class_names, const std::vector<std::string>& bg_names);

	bool draw(MenuSelection& out);

private:
	const std::vector<std::string>& _class_names;
	const std::vector<std::string>& _bg_names;
	int _left_idx = 0;
	int _right_idx = 1;
	int _bg_idx = 0;
};
