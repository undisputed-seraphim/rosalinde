#include "script_state.hpp"
#include "screenshot.hpp"

#include <asb.hpp>
#include <criware/cpk.hpp>

#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <optional>
#include <stdexcept>
#include <unordered_map>

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

Job::dir_and_path split_path(const std::string& p) {
	const auto pos = p.find_last_of('/');
	if (pos == std::string::npos)
		return {"", p};
	return {p.substr(0, pos), p.substr(pos + 1)};
}

const bool kIsSpriteCall(const std::string& c) {
	return c == "CreateSpriteMone" || c == "DrUiCreateChara" || c == "CreateSpriteCoA" ||
		   c == "CreateSpriteEffect" || c == "CreateSpriteMoneWorld" || c == "CreateSpriteMoneStageStart";
}
} // namespace

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

	// The game's VFS is case-insensitive: scripts may reference "GUI/interface.mbs"
	// for a file shipped as "GUI/Interface.mbs". Build a lowercase index for lookup.
	auto lower = [](std::string s) {
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	};
	std::unordered_map<std::string, std::pair<std::string, std::string>> ci;
	for (const auto& [d, f] : cpk.rows<"DirName", "FileName">())
		ci[lower(d + "/" + f)] = {d, f};

	auto lookup = [&](const std::string& path) -> std::optional<std::pair<std::string, std::string>> {
		auto sp = split_path(path);
		if (cpk.find_file(sp.dir, sp.path) != cpk.end())
			return std::make_pair(sp.dir, sp.path);
		auto it = ci.find(lower(path));
		if (it != ci.end())
			return it->second;
		return std::nullopt;
	};
	// Scripts sometimes reference an FTX with a single-digit frame suffix
	// ("Op_Prince_M0.ftx") while the archive ships "Op_Prince_M00.ftx".
	auto lookup_ftx = [&](const std::string& ftx, const std::string& mbs) -> std::optional<std::pair<std::string, std::string>> {
		std::vector<std::string> cands = {ftx};
		auto sp = split_path(ftx);
		std::string n = sp.path;
		auto ext = n.rfind('.');
		if (ext != std::string::npos) {
			size_t e = ext;
			while (e > 0 && std::isdigit(static_cast<unsigned char>(n[e - 1])))
				--e;
			if (e < ext && ext - e == 1)
				cands.push_back(sp.dir.empty() ? (n.substr(0, e) + "0" + n.substr(e))
											   : (sp.dir + "/" + n.substr(0, e) + "0" + n.substr(e)));
		}
		std::string stem = mbs;
		if (stem.size() >= 4 && stem.compare(stem.size() - 4, 4, ".mbs") == 0)
			stem.resize(stem.size() - 4);
		for (const char* suf : {".ftx", "0.ftx", "00.ftx"})
			cands.push_back(stem + suf);
		for (const auto& c : cands)
			if (auto r = lookup(c))
				return r;
		return std::nullopt;
	};
	const auto slash = _cfg.entry.find_last_of('/');
	const std::string dir = slash == std::string::npos ? "" : _cfg.entry.substr(0, slash);
	const std::string file = slash == std::string::npos ? _cfg.entry : _cfg.entry.substr(slash + 1);

	auto entry = cpk.find_file(dir, file);
	if (entry == cpk.end())
		throw std::runtime_error("script not found in CPK: " + _cfg.entry);

	std::vector<char> buf;
	cpk.extract(*entry, buf);
	asb2::Module mod = asb2::parse(
		std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buf.data()), buf.size()));
	asb2::Scene scene = asb2::extract_scene(mod);

	auto find_fn = [&](const std::string& n) -> const asb2::SceneFunction* {
		for (const auto& f : scene.functions)
			if (f.name == n)
				return &f;
		return nullptr;
	};
	const asb2::SceneFunction* fn = nullptr;
	if (!_cfg.func.empty())
		fn = find_fn(_cfg.func);
	if (!fn)
		fn = find_fn("scene00");
	if (!fn)
		fn = find_fn("MAIN");
	if (!fn)
		for (const auto& f : scene.functions)
			if (f.name.rfind("scene", 0) == 0) {
				fn = &f;
				break;
			}
	if (!fn) {
		size_t best = 0;
		for (const auto& f : scene.functions) {
			size_t c = 0;
			for (const auto& e : f.events)
				if (kIsSpriteCall(e.call))
					++c;
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

	asb2::SceneIR ir = asb2::build_scene_ir(*fn, _cfg.entry);
	fprintf(stderr, "[script] %s :: %s  (entities=%zu camera=%zu)\n", _cfg.entry.c_str(), fn->name.c_str(),
			ir.entities.size(), ir.camera.size());

	for (const auto& e : ir.entities) {
		if (_layers.size() >= static_cast<size_t>(std::max(1, _cfg.limit)))
			break;
		if (!e.visible || e.mbs.empty() || e.ftx.empty())
			continue;

		auto m = lookup(e.mbs);
		auto fx = lookup_ftx(e.ftx, e.mbs);
		if (!m || !fx) {
			fprintf(stderr, "[script] skip %s (not in archive)\n", e.mbs.c_str());
			continue;
		}

		Job job;
		job.mbs = {m->first, m->second};
		job.ftx = {fx->first, fx->second};

		std::unique_ptr<SpriteLayer> layer;
		try {
			layer = _loader.load_layer(job, 0, e.id, e.kind);
		} catch (const std::exception& ex) {
			fprintf(stderr, "[script] skip %s: %s\n", e.id.c_str(), ex.what());
			continue;
		}
		if (e.track_index >= 0 && e.track_index < static_cast<int>(layer->data.tracks.size()))
			layer->instance.play(static_cast<uint32_t>(e.track_index));
		layer->position = glm::vec2(e.pos[0], e.pos[1]);
		_layers.push_back(std::move(layer));
	}

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
		const asb2::SceneCamera* key = nullptr;
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
	for (auto& layer : _layers)
		layer->instance.update(dt);
}

void ScriptState::render() {
	++_frame;

	glClear(GL_DEPTH_BUFFER_BIT);
	for (auto& layer : _layers)
		layer->renderer.draw(layer->instance, _projection, _camera, layer->position, &layer->layer_tints);

	if (!_cfg.screenshot.empty() && !_captured && _frame >= _cfg.frames) {
		int fb_w = 0, fb_h = 0;
		SDL_GetWindowSizeInPixels(SDL_GL_GetCurrentWindow(), &fb_w, &fb_h);
		if (fb_w > 0 && fb_h > 0) {
			std::vector<uint8_t> pixels(static_cast<size_t>(fb_w) * fb_h * 4);
			glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
			const bool ok = screenshot::write(_cfg.screenshot, fb_w, fb_h, pixels);
			fprintf(stderr, "%s: %s (%dx%d)\n", ok ? "screenshot written" : "screenshot FAILED",
					_cfg.screenshot.c_str(), fb_w, fb_h);
		}
		_captured = true;
	}
}
