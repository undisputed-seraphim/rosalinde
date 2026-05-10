#include "scene.hpp"
#include "tables.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <glxx/error.hpp>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <spanstream>
#include <stdexcept>

namespace {
	void enable_blend(const glm::vec4 blend) {
		glBlendColor(blend[0], blend[1], blend[2], blend[3]);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
	}

	void enable_depth(GLenum depthFunc = 0) {
		if (depthFunc == 0) {
			glClear(GL_DEPTH_BUFFER_BIT);
			glClearDepth(1.0);
			glDisable(GL_DEPTH_TEST);
			return;
		}
		glDepthFunc(depthFunc);
		glEnable(GL_DEPTH_TEST);
	}
}

void BackgroundScene::load(const Job& job, CPKTable& cpkt) {
	std::vector<char> mbs_buf, ftx_buf;
	if (auto entry = cpkt.find_file(job.mbs.dir, job.mbs.path); entry == cpkt.end()) {
		throw std::runtime_error("BG MBS was not found: " + job.mbs.dir + "/" + job.mbs.path);
	} else {
		cpkt.extract(*entry, mbs_buf);
	}
	if (auto entry = cpkt.find_file(job.ftx.dir, job.ftx.path); entry == cpkt.end()) {
		throw std::runtime_error("BG FTX was not found: " + job.ftx.dir + "/" + job.ftx.path);
	} else {
		cpkt.extract(*entry, ftx_buf);
	}

	auto ftx_entries = FTX::parse(ftx_buf);
	for (auto& t : ftx_entries) {
		FTX::decompress(t);
		FTX::deswizzle(t);
	}

	data = SpriteData::load(
		std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(mbs_buf.data()), mbs_buf.size()),
		std::move(ftx_entries));
	renderer.upload_textures(data);

	const auto& v77 = data.v77;
	for (uint32_t i = 0; i < v77.s9.size(); ++i) {
		const auto& s9 = v77.s9[i];
		if (s9.disabled) continue;

		const auto& sa = v77.sa[s9.sa_set_id];
		if (sa.s8_no == 0) continue;
		const auto& s8 = v77.s8[sa.s8_id + sa.s8_st];
		const auto& s7 = v77.s7[s8.s7_id];
		bool is_far = ((s7.fog >> 24) & 0xFF) == 0xFF;

		auto& el = elements.emplace_back(i, is_far);
		el.instance = SpriteInstance{&data, i, 0xFFFFFFFF};
		el.instance.play(i);
	}
}

void BackgroundScene::update(float dt) {
	for (auto& el : elements) {
		el.instance.update(dt);
	}
}

glm::vec4 BackgroundScene::extent() const {
	float l = INFINITY, t = INFINITY, r = -INFINITY, b = -INFINITY;
	for (const auto& s9 : data.v77.s9) {
		if (s9.disabled) continue;
		l = std::min(l, s9.left);
		t = std::min(t, s9.top);
		r = std::max(r, s9.right);
		b = std::max(b, s9.bottom);
	}
	return {l, t, r, b};
}

std::unique_ptr<SpriteLayer> Scene::load_layer(
	const Job& job, uint32_t trackid,
	const std::string& class_name, const std::string& variant_name) const {

	std::vector<char> mbs_buf, ftx_buf;
	if (auto entry = _cpkt.find_file(job.mbs.dir, job.mbs.path); entry == _cpkt.end()) {
		throw std::runtime_error("MBS was not found: " + job.mbs.dir + "/" + job.mbs.path);
	} else {
		_cpkt.extract(*entry, mbs_buf);
	}
	if (auto entry = _cpkt.find_file(job.ftx.dir, job.ftx.path); entry == _cpkt.end()) {
		throw std::runtime_error("FTX was not found: " + job.ftx.dir + "/" + job.ftx.path);
	} else {
		_cpkt.extract(*entry, ftx_buf);
	}

	auto ftx_entries = FTX::parse(ftx_buf);
	for (auto& t : ftx_entries) {
		FTX::decompress(t);
		FTX::deswizzle(t);
	}

	auto layer = std::make_unique<SpriteLayer>();
	layer->name = class_name + ":" + variant_name;
	layer->class_name = class_name;
	layer->variant_name = variant_name;
	layer->default_flags = 0xFFFFFFFF;
	layer->data = SpriteData::load(
		std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(mbs_buf.data()), mbs_buf.size()),
		std::move(ftx_entries));
	for (const auto& s4 : layer->data.v77.s4)
		layer->layer_tints.try_emplace(s4.attributes, 1.0f, 1.0f, 1.0f, 1.0f);
	layer->renderer.upload_textures(layer->data);
	layer->instance = SpriteInstance{&layer->data, trackid, layer->default_flags};
	layer->instance.play(trackid);
	return layer;
}

Scene::Scene(std::filesystem::path cpkpath,
	const std::string& classname,
	const std::string& charaname,
	const std::string& classname2,
	const std::string& charaname2,
	uint32_t trackid,
	bool debug,
	std::string screenshot_path)
	: _cpkt(TopLevelCpk(cpkpath).getTableOfContents())
	, _camera(2.5)
	, _projection(1.0)
	, _screenshot_path(std::move(screenshot_path)) {

	if (debug) {
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(&message_callback, NULL);
	}

	printf(" Version: %s\n", glGetString(GL_VERSION));
	printf("  Vendor: %s\n", glGetString(GL_VENDOR));
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf(" Shading: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	enable_blend(glm::vec4(1.0, 1.0, 1.0, 1.0));
	enable_depth(GL_ALWAYS);

	static constexpr int W = 1920, H = 1080;
	_projection = glm::ortho((-W) / 2.0f, W / 2.0f, H / 2.0f, (-H) / 2.0f);

	{
		const auto bg_iter = BattleBGs.find("BGBtCathedral");
		if (bg_iter == BattleBGs.end()) {
			throw std::runtime_error("BGBtCathedral not found in BattleBGs table.");
		}
		std::cout << bg_iter->second.mbs.dir << '\t' << bg_iter->second.mbs.path << '\n';
		_background.load(bg_iter->second, _cpkt);

		auto ext = _background.extent();
		_camera.fit_bounds(ext);

		float w = ext.z - ext.x;
		float ground = ext.w;
		float cam_y = (ext.y + ext.w) / 2.0f;
		_left_pos = glm::vec2(-w * 0.25f, ground - cam_y);
		_right_pos = glm::vec2(w * 0.25f, ground - cam_y);
	}

	const auto iter = Characters.find(classname);
	if (iter == Characters.end()) {
		throw std::runtime_error("Entry for character class " + classname + " was not found.");
	}

	const auto& job = iter->second;
	(void)iter->second.variants.at(charaname);
	std::cout << job.mbs.dir << '\t' << job.mbs.path << '\n';

	_layers.push_back(load_layer(job, trackid, classname, charaname));
	_layers.back()->position = _left_pos;

	for (const auto& [name, _] : Characters) {
		_class_names.push_back(name);
	}

	if (!classname2.empty()) {
		const auto iter2 = Characters.find(classname2);
		if (iter2 == Characters.end()) {
			throw std::runtime_error("Entry for character class " + classname2 + " was not found.");
		}
		(void)iter2->second.variants.at(charaname2);
		_layers.push_back(load_layer(iter2->second, 0, classname2, charaname2));
		_layers.back()->position = _right_pos;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();

	ImGui_ImplSDL3_InitForOpenGL(SDL_GL_GetCurrentWindow(), SDL_GL_GetCurrentContext());
	ImGui_ImplOpenGL3_Init("#version 450");
}

Scene::~Scene() noexcept {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

bool Scene::handle_inputs() {
	SDL_Event event{};
	bool done = _done;
	while (SDL_PollEvent(&event)) {
		ImGui_ImplSDL3_ProcessEvent(&event);

		if (event.type == SDL_EVENT_QUIT) {
			done = true;
		}
		if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
			done = true;
		}

		if (!ImGui::GetIO().WantCaptureMouse) {
			_camera.handleInput(event);
		}

		if (ImGui::GetIO().WantCaptureKeyboard) continue;

		switch (event.type) {
		case SDL_EVENT_KEY_DOWN: {
			if (_layers.empty()) break;

			const bool shift = event.key.mod & SDL_KMOD_SHIFT;
			if (shift && _layers.size() > 1) {
				auto& bg = _layers[1]->instance;
				switch (event.key.key) {
				case SDLK_DOWN: bg.prev_track(); break;
				case SDLK_UP:   bg.next_track(); break;
				}
			} else {
				auto& inst = _layers[_active_layer]->instance;
				switch (event.key.key) {
			case SDLK_DOWN: inst.prev_track(); break;
			case SDLK_UP:   inst.next_track(); break;
				}
			}
			break;
		}
		}
	}
	return done;
}

void Scene::render() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320, 500), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Characters")) {
		static int side = 0;
		ImGui::RadioButton("Left", &side, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Right", &side, 1);
		ImGui::SameLine();
		if (_layers.size() > 1 && ImGui::Button("Swap")) {
			std::swap(_layers[0]->position, _layers[1]->position);
			std::swap(_layers[0], _layers[1]);
		}

		if (side < (int)_layers.size()) {
			ImGui::Text("Active: %s", _layers[side]->name.c_str());
		}
		ImGui::Separator();

		for (const auto& name : _class_names) {
			bool selected = false;
			if (side < (int)_layers.size()) {
				selected = (_layers[side]->class_name == name);
			}
			if (ImGui::Selectable(name.c_str(), selected)) {
				const auto& job = Characters.at(name);
				auto var_it = job.variants.find(_layers[side]->variant_name);
				if (var_it == job.variants.end()) var_it = job.variants.begin();
		_layers[side] = load_layer(job, 0, name, var_it->first);
			_layers[side]->position = side == 0 ? _left_pos : _right_pos;
			}
		}
	}
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(10, 520), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320, 500), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Animations") && !_layers.empty()) {
		static int anim_side = 0;
		ImGui::RadioButton("Left##anim", &anim_side, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Right##anim", &anim_side, 1);

		auto& layer = _layers[anim_side < (int)_layers.size() ? anim_side : 0];
		ImGui::Text("%s", layer->name.c_str());
		ImGui::Separator();

		char filter[64] = {};
		ImGui::InputText("Filter", filter, sizeof(filter));
		std::string f(filter);

		for (uint32_t i = 0; i < layer->data.tracks.size(); ++i) {
			const auto& track = layer->data.tracks[i];
			if (track.name.empty()) continue;
			if (!f.empty() && track.name.find(f) == std::string::npos) continue;

		if (ImGui::Selectable(track.name.c_str(), layer->instance.track_idx == i)) {
			layer->instance.play(i);
			}
		}
	}
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(10, 960), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(1900, 100), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Variant Flags") && !_layers.empty()) {
		ImGui::RadioButton("Left##vflags", &_variant_side, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Right##vflags", &_variant_side, 1);

		auto& layer = _layers[_variant_side < (int)_layers.size() ? _variant_side : 0];
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
	if (ImGui::Begin("Layer Tints") && !_layers.empty()) {
		auto& layer = _layers[_variant_side < (int)_layers.size() ? _variant_side : 0];
		for (auto& [attrs, tint] : layer->layer_tints) {
			char label[12];
			snprintf(label, sizeof(label), "%08X", attrs);
			ImGui::ColorEdit4(label, &tint[0],
				ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex);
		}
	}
	ImGui::End();

	ImGui::Render();

	for (auto& el : _background.elements) {
		if (!el.is_far) continue;
		_background.renderer.draw(el.instance, _projection, _camera, {0.0f, 0.0f});
	}

	for (auto& layer : _layers) {
		layer->renderer.draw(layer->instance, _projection, _camera, layer->position, &layer->layer_tints);
	}

	for (auto& el : _background.elements) {
		if (el.is_far) continue;
		_background.renderer.draw(el.instance, _projection, _camera, {0.0f, 0.0f});
	}

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (!_screenshot_path.empty() && !_captured) {
		static constexpr int W = 1920, H = 1080;
		std::vector<uint8_t> pixels(W * H * 4);
		glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

		FILE* f = fopen(_screenshot_path.c_str(), "wb");
		if (f) {
			fprintf(f, "P6\n%d %d\n255\n", W, H);
			for (int y = H - 1; y >= 0; --y) {
				for (int x = 0; x < W; ++x) {
					unsigned i = (y * W + x) * 4;
					fwrite(&pixels[i], 1, 3, f);
				}
			}
			fclose(f);
		}
		_captured = true;
		_done = true;
	}
}

void Scene::update(float dt) {
	_background.update(dt);
	for (auto& layer : _layers) {
		layer->instance.update(dt);
	}
}
