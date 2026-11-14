#include "debug_ui.hpp"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <spdlog/spdlog.h>

DebugUI::DebugUI() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();

	const char* font_env = std::getenv("VIEWER_FONT");
	static const ImWchar latin_ranges[] = {0x0020, 0x00FF, 0};
	static const ImWchar cjk_ranges[] = {0x2000, 0x30FF, 0x3400, 0x4DBF, 0x4E00, 0x9FFF, 0xFF00, 0xFFEF, 0};
	static const ImWchar all_ranges[] = {0x0020, 0x00FF, 0x2000, 0x30FF, 0x3400, 0x4DBF, 0x4E00, 0x9FFF, 0xFF00, 0xFFEF, 0};
	ImFontConfig font_cfg;
	font_cfg.OversampleH = 1;
	font_cfg.OversampleV = 1;

	// DroidSansFallbackFull is CJK-only, so ASCII text (English dialogue, the
	// stats overlay) needs a separate Latin base font. Merge the CJK fallback
	// into it for Japanese/Chinese glyphs.
	if (font_env && *font_env && std::filesystem::exists(font_env) &&
		ImGui::GetIO().Fonts->AddFontFromFileTTF(font_env, 24.0f, &font_cfg, all_ranges)) {
		spdlog::debug("font: {}", font_env);
	} else {
		const char* latin_paths[] = {
			"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
			"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
			"C:/Windows/Fonts/segoeui.ttf",
			"C:/Windows/Fonts/arial.ttf",
		};
		bool have_base = false;
		for (const char* path : latin_paths) {
			if (!std::filesystem::exists(path))
				continue;
			if (ImGui::GetIO().Fonts->AddFontFromFileTTF(path, 24.0f, &font_cfg, latin_ranges)) {
				spdlog::debug("font: base {}", path);
				have_base = true;
				break;
			}
		}
		if (!have_base)
			ImGui::GetIO().Fonts->AddFontDefault();

		const char* cjk_paths[] = {
			"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
			"/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
			"/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
			"C:/Windows/Fonts/msgothic.ttc",
			"C:/Windows/Fonts/meiryo.ttc",
		};
		ImFontConfig merge_cfg = font_cfg;
		merge_cfg.MergeMode = true;
		for (const char* path : cjk_paths) {
			if (!std::filesystem::exists(path))
				continue;
			if (ImGui::GetIO().Fonts->AddFontFromFileTTF(path, 24.0f, &merge_cfg, cjk_ranges)) {
				spdlog::debug("font: cjk {}", path);
				break;
			}
		}
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

void DebugUI::draw_stats(float fps, float frame_ms, bool vsync) {
	if (!_stats_visible)
		return;
	char line[96];
	std::snprintf(line, sizeof(line), "%.1f fps  %.2f ms  vsync %s", fps, frame_ms, vsync ? "on" : "off");
	ImDrawList* draw = ImGui::GetForegroundDrawList();
	const ImVec2 pos(12.0f, 10.0f);
	const ImVec2 size = ImGui::CalcTextSize(line);
	draw->AddRectFilled(
		ImVec2(pos.x - 6.0f, pos.y - 4.0f),
		ImVec2(pos.x + size.x + 6.0f, pos.y + size.y + 4.0f),
		IM_COL32(0, 0, 0, 150),
		4.0f);
	draw->AddText(pos, IM_COL32(255, 255, 255, 255), line);
}
