#include "battle_state.hpp"
#include "screenshot.hpp"
#include "tables.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <glxx/error.hpp>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <iterator>
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

glm::vec4 world_bounds(const SpriteLayer& layer) {
	const glm::vec4 b = layer.instance.track_bounds();
	return {b.x + layer.position.x, b.y + layer.position.y, b.z + layer.position.x, b.w + layer.position.y};
}
} // namespace

BattleState::BattleState(
	const AssetLoader& loader,
	DebugUI& ui,
	const MenuSelection& selection,
	std::string screenshot_path,
	uint32_t capture_frame)
	: _loader(loader)
	, _ui(ui)
	, _camera(2.5f)
	, _projection(1.0f)
	, _screenshot_path(std::move(screenshot_path))
	, _capture_frame(capture_frame) {

	static constexpr int W = 1920, H = 1080;
	_projection = glm::ortho((-W) / 2.0f, W / 2.0f, H / 2.0f, (-H) / 2.0f);

	enable_blend(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	enable_depth(GL_ALWAYS);

	load_characters(selection);
	load_background(selection.background);

	if (_has_background) {
		// TEMP: BG_CAM=<track_idx> fits the camera to that element's s9 bounds.
		const char* cam = std::getenv("BG_CAM");
		if (cam) {
			const int idx = std::atoi(cam);
			const auto& s9 = _background.data.v77.s9[idx];
			_camera.fit_bounds({s9.left, s9.top, s9.right, s9.bottom}, 0.02f);
		} else {
			_camera.fit_bounds(_background.extent(), 0.02f);
		}
	}
}

void BattleState::load_background(const std::string& name) {
	if (name.empty())
		return;
	const auto it = BattleBGs.find(name);
	if (it == BattleBGs.end())
		throw std::runtime_error("Battle BG " + name + " was not found.");
	_background = _loader.load_background(it->second);
	_background.rebind();
	_has_background = true;
}

void BattleState::load_characters(const MenuSelection& selection) {
	_layers.clear();

	{
		const auto iter = Characters.find(selection.left_class);
		if (iter == Characters.end())
			throw std::runtime_error("Entry for character class " + selection.left_class + " was not found.");
		const auto& job = iter->second;
		auto var_it = job.variants.begin();

		auto track_idx = _layers.empty() ? 0 : 0;
		_layers.push_back(_loader.load_layer(job, track_idx, selection.left_class, var_it->first));
		_layers.back()->position = glm::vec2(-300.0f, 0.0f);
	}

	if (!selection.right_class.empty()) {
		const auto iter2 = Characters.find(selection.right_class);
		if (iter2 != Characters.end()) {
			const auto& job = iter2->second;
			auto var_it = job.variants.begin();
			_layers.push_back(_loader.load_layer(job, 0, selection.right_class, var_it->first));
			_layers.back()->position = glm::vec2(300.0f, 0.0f);
		}
	}

	if (!_layers.empty()) {
		glm::vec4 fit = world_bounds(*_layers.front());
		for (size_t i = 1; i < _layers.size(); ++i) {
			const glm::vec4 b = world_bounds(*_layers[i]);
			fit.x = std::min(fit.x, b.x);
			fit.y = std::min(fit.y, b.y);
			fit.z = std::max(fit.z, b.z);
			fit.w = std::max(fit.w, b.w);
		}
		_camera.fit_bounds(fit, 0.15f);
	}
}

void BattleState::apply_action(AnimationGraph::Action action) {
	if (_layers.empty())
		return;

	auto* phase = _graph.resolve(action);
	if (!phase)
		return;
	_graph.begin(*phase);

	auto& layer = _layers[_active_layer];
	layer->instance.play(phase->track_name);
}

bool BattleState::handle_inputs() {
	SDL_Event event{};
	while (SDL_PollEvent(&event)) {
		ImGui_ImplSDL3_ProcessEvent(&event);

		if (event.type == SDL_EVENT_QUIT)
			_quit = true;
		if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
			_quit = true;

		if (!ImGui::GetIO().WantCaptureMouse) {
			_camera.handleInput(event);
		}

		if (ImGui::GetIO().WantCaptureKeyboard)
			continue;

		switch (event.type) {
		case SDL_EVENT_KEY_DOWN: {
			if (_layers.empty())
				break;

			switch (event.key.key) {
			case SDLK_RIGHT:
			case SDLK_D:
				apply_action(AnimationGraph::Action::WalkRight);
				break;
			case SDLK_LEFT:
			case SDLK_A:
				apply_action(AnimationGraph::Action::WalkLeft);
				break;
			case SDLK_J:
			case SDLK_SPACE:
				apply_action(AnimationGraph::Action::Attack);
				break;
			case SDLK_UP:
				if (event.key.mod & SDL_KMOD_SHIFT && _layers.size() > 1)
					_layers[1]->instance.next_track();
				else
					_layers[_active_layer]->instance.next_track();
				break;
			case SDLK_DOWN:
				if (event.key.mod & SDL_KMOD_SHIFT && _layers.size() > 1)
					_layers[1]->instance.prev_track();
				else
					_layers[_active_layer]->instance.prev_track();
				break;
			case SDLK_ESCAPE:
				_done = true;
				break;
			}
			break;
		}
		case SDL_EVENT_KEY_UP: {
			switch (event.key.key) {
			case SDLK_RIGHT:
			case SDLK_D:
			case SDLK_LEFT:
			case SDLK_A:
				apply_action(AnimationGraph::Action::Idle);
				break;
			}
			break;
		}
		}
	}
	return _done;
}

void BattleState::update(float dt) {
	auto [phase, changed] = _graph.tick(dt);
	if (changed && phase && !_layers.empty()) {
		auto& layer = _layers[_active_layer];
		layer->instance.play(phase->track_name);
	}

	static constexpr float kMoveSpeed = 300.0f;
	if (!_layers.empty()) {
		auto& layer = _layers[_active_layer];
		auto action = [&]() -> AnimationGraph::Action {
			const bool* keys = SDL_GetKeyboardState(nullptr);
			if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D])
				return AnimationGraph::Action::WalkRight;
			if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A])
				return AnimationGraph::Action::WalkLeft;
			return AnimationGraph::Action::Idle;
		}();

		if (action != AnimationGraph::Action::Idle) {
			auto* phase = _graph.current_phase();
			bool in_attack = phase && phase->track_name.rfind("MAGIC_A", 0) == 0;
			if (!in_attack) {
				float dir = (action == AnimationGraph::Action::WalkRight) ? 1.0f : -1.0f;
				layer->position.x += dir * kMoveSpeed * dt;
			}
		}
	}

	if (!_layers.empty()) {
		glm::vec2 mid{0.0f, 0.0f};
		for (const auto& layer : _layers) {
			const glm::vec4 b = world_bounds(*layer);
			mid += glm::vec2{(b.x + b.z) * 0.5f, (b.y + b.w) * 0.5f};
		}
		if (!_has_background)
			_camera.set_target(mid / static_cast<float>(_layers.size()));
	}
	_camera.update_follow(dt);

	// TEMP HACK: stage enabled; keep the background animating.
	if (_has_background)
		_background.update(dt);

	for (auto& layer : _layers) {
		layer->instance.update(dt);
	}
}

void BattleState::render() {
	++_frame;

	// TEMP HACK: stage only. Character drawing and ImGui menus disabled.
	// TEMP: BG_ONLY=<track_idx> renders a single background element in isolation.
	static const int only = [] {
		const char* s = std::getenv("BG_ONLY");
		return s ? std::atoi(s) : -1;
	}();
	// TEMP: BG_COLOR tints each element a distinct color so it can be identified.
	static const bool colorize = std::getenv("BG_COLOR") != nullptr;
	static const bool no_fog = std::getenv("BG_NOFOG") != nullptr;
	static const std::vector<uint32_t> all_attrs = [&] {
		std::vector<uint32_t> a;
		for (const auto& s4 : _background.data.v77.s4)
			a.push_back(s4.attributes);
		return a;
	}();
	static const glm::vec4 palette[] = {
		{1, 0, 0, 1},
		{0, 1, 0, 1},
		{0, 0, 1, 1},
		{1, 1, 0, 1},
		{1, 0, 1, 1},
		{0, 1, 1, 1},
		{1, 0.5f, 0, 1},
		{0.5f, 0, 1, 1},
		{0, 1, 0.5f, 1},
		{0.5f, 1, 0, 1},
		{1, 0, 0.5f, 1},
		{0.5f, 0.5f, 1, 1},
		{1, 1, 1, 1}};
	auto draw_bg = [&](bool far_pass) {
		for (const auto& el : _background.elements) {
			if (el.is_far != far_pass)
				continue;
			if (only >= 0 && static_cast<int>(el.track_idx) != only)
				continue;
			if (colorize) {
				std::map<uint32_t, glm::vec4> tints;
				const glm::vec4 c = palette[el.track_idx % std::size(palette)];
				for (uint32_t a : all_attrs)
					tints[a] = c;
				_background.renderer.draw(el.instance, _projection, _camera, {0.0f, 0.0f}, &tints, !no_fog);
			} else {
				_background.renderer.draw(el.instance, _projection, _camera, {0.0f, 0.0f}, nullptr, !no_fog);
			}
		}
	};

	if (_has_background)
		draw_bg(true);

	// TEMP HACK: characters disabled
	// for (auto& layer : _layers) {
	// 	layer->renderer.draw(layer->instance, _projection, _camera, layer->position, &layer->layer_tints);
	// }

	if (_has_background)
		draw_bg(false);

	// TEMP HACK: ImGui menus disabled
	// _ui.draw(_layers, _loader.class_names(), _loader, _variant_side);

	if (!_screenshot_path.empty() && !_captured && _frame >= _capture_frame) {
		int fb_w = 0, fb_h = 0;
		SDL_GetWindowSizeInPixels(SDL_GL_GetCurrentWindow(), &fb_w, &fb_h);
		if (fb_w > 0 && fb_h > 0) {
			std::vector<uint8_t> pixels(static_cast<size_t>(fb_w) * fb_h * 4);
			glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
			bool ok = screenshot::write(_screenshot_path, fb_w, fb_h, pixels);
			fprintf(
				stderr,
				"%s: %s (%dx%d)\n",
				ok ? "screenshot written" : "screenshot FAILED",
				_screenshot_path.c_str(),
				fb_w,
				fb_h);
		}
		_captured = true;
	}
}
