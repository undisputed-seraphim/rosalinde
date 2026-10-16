#include "script_state.hpp"
#include "scene_layers.hpp"
#include "scene_util.hpp"
#include "screenshot.hpp"

#include <criware/cpk.hpp>
#include <scarlett/asb.hpp>

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <stdexcept>

ScriptState::ScriptState(const AssetLoader& loader, DebugUI& ui, ScriptSceneConfig cfg)
	: _loader(loader)
	, _ui(ui)
	, _camera(2.5f)
	, _projection(1.0f)
	, _cfg(std::move(cfg)) {

	_projection = glm::ortho(-960.0f, 960.0f, 540.0f, -540.0f);
	enable_blend(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	enable_depth(GL_ALWAYS);

	const CPKTable& cpk = _loader.cpk();

	const auto slash = _cfg.entry.find_last_of('/');
	const std::string dir = slash == std::string::npos ? "" : _cfg.entry.substr(0, slash);
	const std::string file = slash == std::string::npos ? _cfg.entry : _cfg.entry.substr(slash + 1);

	auto entry = cpk.find_file(dir, file);
	if (entry == cpk.end())
		throw std::runtime_error("script not found in CPK: " + _cfg.entry);

	std::vector<char> buf;
	cpk.extract(*entry, buf);
	asb::Module mod = asb::parse(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buf.data()), buf.size()));
	asb::Scene scene = asb::extract_scene(mod);

	auto find_fn = [&](const std::string& n) -> const asb::SceneFunction* {
		for (const auto& f : scene.functions)
			if (f.name == n)
				return &f;
		return nullptr;
	};
	auto sprites = [](const asb::SceneFunction& f) {
		size_t c = 0;
		for (const auto& e : f.events)
			if (asb::is_entity_create_call(e.call))
				++c;
		return c;
	};

	const asb::SceneFunction* fn = nullptr;
	if (!_cfg.func.empty())
		fn = find_fn(_cfg.func);
	if (!fn)
		for (const char* nm : {"scene00", "MAIN"}) {
			const asb::SceneFunction* c = find_fn(nm);
			if (c && sprites(*c) > 0) {
				fn = c;
				break;
			}
		}
	if (!fn)
		for (const auto& f : scene.functions)
			if (f.name.rfind("scene", 0) == 0 && sprites(f) > 0) {
				fn = &f;
				break;
			}
	if (!fn) {
		size_t best = 0;
		for (const auto& f : scene.functions) {
			size_t c = sprites(f);
			if (c > best) {
				best = c;
				fn = &f;
			}
		}
	}
	if (!fn)
		for (const auto& f : scene.functions)
			if (!f.events.empty()) {
				fn = &f;
				break;
			}
	if (!fn)
		throw std::runtime_error("no scene function found in " + _cfg.entry);

	asb::CharTable chars;
	if (!_cfg.chara_table.empty())
		chars = asb::load_char_table(_cfg.chara_table);
	asb::SceneIR ir = asb::build_scene_ir(*fn, _cfg.entry, chars.empty() ? nullptr : &chars);
	fprintf(
		stderr,
		"[script] %s :: %s  (entities=%zu camera=%zu)\n",
		_cfg.entry.c_str(),
		fn->name.c_str(),
		ir.entities.size(),
		ir.camera.size());

	std::vector<LayerSpec> specs;
	specs.reserve(ir.entities.size());
	for (const auto& e : ir.entities) {
		LayerSpec s;
		s.id = e.id;
		s.kind = e.kind;
		s.mbs = e.mbs;
		s.ftx = e.ftx;
		s.track_index = e.track_index;
		s.pos = glm::vec2(e.pos[0], e.pos[1]);
		s.scale = e.scale;
		s.visible = e.visible;
		specs.push_back(std::move(s));
	}
	for (auto& layer : build_layers(_loader, specs, std::max(1, _cfg.limit)))
		if (layer)
			_layers.push_back(std::move(layer));

	fprintf(stderr, "[script] loaded %zu sprite layer(s)\n", _layers.size());
	if (_layers.empty())
		throw std::runtime_error("scene produced no loadable sprites: " + _cfg.entry);

	glm::vec4 fit = world_bounds(*_layers[0]);
	for (size_t i = 1; i < _layers.size(); ++i) {
		const glm::vec4 b = world_bounds(*_layers[i]);
		fit = {std::min(fit.x, b.x), std::min(fit.y, b.y), std::max(fit.z, b.z), std::max(fit.w, b.w)};
	}
	_camera.fit_bounds(fit, 0.15f);

	if (_cfg.camera != "fit" && !ir.camera.empty()) {
		const asb::SceneCamera* key = nullptr;
		if (_cfg.camera == "first")
			key = &ir.camera.front();
		else if (_cfg.camera == "last")
			key = &ir.camera.back();
		else {
			const int n = std::atoi(_cfg.camera.c_str());
			if (n >= 0 && n < static_cast<int>(ir.camera.size()))
				key = &ir.camera[n];
		}
		if (key) {
			float s = 1.0f;
			if (const char* e = std::getenv("SCENE_CAM_SCALE"))
				s = std::atof(e);
			_camera.recenter({key->pos[0] * s, key->pos[1] * s});
			fprintf(stderr, "[script] camera key pos=(%.0f,%.0f)\n", key->pos[0], key->pos[1]);
		}
	}
}

bool ScriptState::handle_inputs() {
	SDL_Event event{};
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
			_quit = true;
		if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
			_done = true;
	}
	return _done;
}

void ScriptState::update(float dt) {
	for (auto& layer : _layers) {
		if (layer->instances.empty()) {
			layer->instance.update(dt);
		} else {
			for (auto& inst : layer->instances)
				inst.update(dt);
		}
	}
}

void ScriptState::render() {
	++_frame;

	glClear(GL_DEPTH_BUFFER_BIT);
	for (auto& layer : _layers) {
		if (layer->instances.empty()) {
			layer->renderer.draw(layer->instance, _projection, _camera, layer->position, &layer->layer_tints);
		} else {
			for (auto& inst : layer->instances)
				layer->renderer.draw(inst, _projection, _camera, layer->position, &layer->layer_tints);
		}
	}

	if (!_cfg.screenshot.empty() && !_captured && _frame >= _cfg.frames) {
		int fb_w = 0, fb_h = 0;
		SDL_GetWindowSizeInPixels(SDL_GL_GetCurrentWindow(), &fb_w, &fb_h);
		if (fb_w > 0 && fb_h > 0) {
			std::vector<uint8_t> pixels(static_cast<size_t>(fb_w) * fb_h * 4);
			glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
			const bool ok = screenshot::write(_cfg.screenshot, fb_w, fb_h, pixels);
			fprintf(
				stderr,
				"%s: %s (%dx%d)\n",
				ok ? "screenshot written" : "screenshot FAILED",
				_cfg.screenshot.c_str(),
				fb_w,
				fb_h);
		}
		_captured = true;
	}
}
