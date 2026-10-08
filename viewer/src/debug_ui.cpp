#include "debug_ui.hpp"
#include "tables.hpp"

#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

DebugUI::DebugUI() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();

	ImGui_ImplSDL3_InitForOpenGL(SDL_GL_GetCurrentWindow(), SDL_GL_GetCurrentContext());
	ImGui_ImplOpenGL3_Init("#version 450");
}

DebugUI::~DebugUI() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

void DebugUI::begin_frame() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void DebugUI::draw(
	std::vector<std::unique_ptr<SpriteLayer>>& layers,
	const std::vector<std::string>& class_names,
	const AssetLoader& loader,
	int& variant_side) {

	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320, 500), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Characters")) {
		static int side = 0;
		ImGui::RadioButton("Left", &side, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Right", &side, 1);
		ImGui::SameLine();
		if (layers.size() > 1 && ImGui::Button("Swap")) {
			std::swap(layers[0]->position, layers[1]->position);
			std::swap(layers[0], layers[1]);
		}

		if (side < (int)layers.size()) {
			ImGui::Text("Active: %s", layers[side]->name.c_str());
		}
		ImGui::Separator();

		for (const auto& name : class_names) {
			bool selected = false;
			if (side < (int)layers.size()) {
				selected = (layers[side]->class_name == name);
			}
			if (ImGui::Selectable(name.c_str(), selected)) {
				const auto& job = Characters.at(name);
				auto var_it = job.variants.find(layers[side]->variant_name);
				if (var_it == job.variants.end())
					var_it = job.variants.begin();
				layers[side] = loader.load_layer(job, 0, name, var_it->first);
				layers[side]->position = side == 0 ? glm::vec2(-300.0f, 0.0f) : glm::vec2(300.0f, 0.0f);
			}
		}
	}
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(10, 520), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320, 500), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Animations") && !layers.empty()) {
		static int anim_side = 0;
		ImGui::RadioButton("Left##anim", &anim_side, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Right##anim", &anim_side, 1);

		auto& layer = layers[anim_side < (int)layers.size() ? anim_side : 0];
		ImGui::Text("%s", layer->name.c_str());
		ImGui::Separator();

		char filter[64] = {};
		ImGui::InputText("Filter", filter, sizeof(filter));
		std::string f(filter);

		for (uint32_t i = 0; i < layer->data.tracks.size(); ++i) {
			const auto& track = layer->data.tracks[i];
			if (track.name.empty())
				continue;
			if (!f.empty() && track.name.find(f) == std::string::npos)
				continue;

			if (ImGui::Selectable(track.name.c_str(), layer->instance.track_idx == i)) {
				layer->instance.play(i);
			}
		}
	}
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(10, 960), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(1900, 100), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Variant Flags") && !layers.empty()) {
		ImGui::RadioButton("Left##vflags", &variant_side, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Right##vflags", &variant_side, 1);

		auto& layer = layers[variant_side < (int)layers.size() ? variant_side : 0];
		auto& flags = layer->instance.variant_flags;

		ImGui::SameLine();
		ImGui::Text(" 0x%08X", flags);
		ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			flags = layer->default_flags;
			for (auto& [_, tint] : layer->layer_tints)
				tint = {1.0f, 1.0f, 1.0f, 1.0f};
			fprintf(stdout, "%s: flags = 0x%08X\n", layer->name.c_str(), flags);
			fflush(stdout);
		}

		for (uint32_t bit : layer->data.attribute_bits) {
			bool on = flags & bit;
			char label[12];
			snprintf(label, sizeof(label), "0x%08X", bit);
			ImGui::SameLine();
			if (ImGui::Checkbox(label, &on))
				flags ^= bit;
		}
	}
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(1330, 10), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(580, 600), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Layer Tints") && !layers.empty()) {
		auto& layer = layers[variant_side < (int)layers.size() ? variant_side : 0];
		for (auto& [attrs, tint] : layer->layer_tints) {
			char label[12];
			snprintf(label, sizeof(label), "%08X", attrs);
			ImGui::ColorEdit4(label, &tint[0], ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex);
		}
	}
	ImGui::End();
}

void DebugUI::end_frame() { ImGui::Render(); }
