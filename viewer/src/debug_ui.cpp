#include "debug_ui.hpp"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

DebugUI::DebugUI() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();

	const char* font_env = std::getenv("VIEWER_FONT");
	const char* font_paths[] = {
		font_env,
		"/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
		"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
		"C:/Windows/Fonts/msgothic.ttc",
	};
	static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2000, 0x30FF, 0x3400, 0x4DBF, 0x4E00, 0x9FFF, 0xFF00, 0xFFEF, 0};
	ImFontConfig font_cfg;
	font_cfg.OversampleH = 1;
	font_cfg.OversampleV = 1;
	for (const char* path : font_paths) {
		if (!path || !*path || !std::filesystem::exists(path))
			continue;
		if (ImGui::GetIO().Fonts->AddFontFromFileTTF(path, 24.0f, &font_cfg, ranges))
			break;
	}

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

void DebugUI::end_frame() { ImGui::Render(); }
