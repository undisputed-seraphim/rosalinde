#include "scene.hpp"
#include "tables.hpp"

#include <SDL3/SDL.h>
#include <cstdio>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <glxx/error.hpp>
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

std::unique_ptr<SpriteLayer> Scene::load_layer(
	const Job& job, uint32_t variant_flags, uint32_t trackid) const {

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
	layer->data = SpriteData::load(
		std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(mbs_buf.data()), mbs_buf.size()),
		std::move(ftx_entries));
	layer->renderer.upload_textures(layer->data);
	layer->instance = SpriteInstance{&layer->data, trackid, variant_flags};
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

	const auto iter = Characters.find(classname);
	if (iter == Characters.end()) {
		throw std::runtime_error("Entry for character class " + classname + " was not found.");
	}

	const auto& job = iter->second;
	auto flags = iter->second.variants.at(charaname);
	std::cout << job.mbs.dir << '\t' << job.mbs.path << '\n';

	_layers.push_back(load_layer(job, flags, trackid));
	_layers.back()->position = glm::vec2(-300.0f, 0.0f);
	_camera.fit_bounds(_layers.back()->instance.track_bounds());

	if (!classname2.empty()) {
		const auto iter2 = Characters.find(classname2);
		if (iter2 == Characters.end()) {
			throw std::runtime_error("Entry for character class " + classname2 + " was not found.");
		}
		auto flags2 = iter2->second.variants.at(charaname2);
		_layers.push_back(load_layer(iter2->second, flags2, 0));
		_layers.back()->position = glm::vec2(300.0f, 0.0f);
	}
}

Scene::~Scene() noexcept {}

bool Scene::handle_inputs() {
	SDL_Event event{};
	bool done = _done;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			done = true;
		}
		if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
			done = true;
		}
		_camera.handleInput(event);
		switch (event.type) {
		case SDL_EVENT_KEY_DOWN: {
			const bool shift = event.key.mod & SDL_KMOD_SHIFT;
			if (shift && _layers.size() > 1) {
				auto& bg = _layers[1]->instance;
				switch (event.key.key) {
				case SDLK_DOWN: bg.prev_track(); break;
				case SDLK_UP:   bg.next_track(); break;
				}
			} else if (!_layers.empty()) {
				auto& inst = _layers[_active_layer]->instance;
				switch (event.key.key) {
				case SDLK_DOWN: inst.prev_track(); _camera.fit_bounds(inst.track_bounds()); break;
				case SDLK_UP:   inst.next_track(); _camera.fit_bounds(inst.track_bounds()); break;
				}
			}
			break;
		}
		}
	}
	return done;
}

void Scene::render() {
	for (auto& layer : _layers) {
		layer->renderer.draw(layer->instance, _projection, _camera, layer->position);
	}

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
	for (auto& layer : _layers) {
		layer->instance.update(dt);
	}
}
