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

Scene::Scene(std::filesystem::path cpkpath,
	const std::string& classname,
	const std::string& charaname,
	uint32_t trackid,
	bool debug)
	: _cpkt(TopLevelCpk(cpkpath).getTableOfContents())
	, _camera(2.5)
	, _projection(1.0)
	, _vao(0) {

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

	glGenVertexArrays(1, &_vao);
	glBindVertexArray(_vao);

	loadSprite(classname, charaname, trackid);
}

Scene::~Scene() noexcept {
	glDeleteVertexArrays(1, &_vao);
}

void Scene::loadSprite(const std::string& classname, const std::string& charaname, uint32_t trackid) {
	const auto iter = Characters.find(classname);
	if (iter == Characters.end()) {
		throw std::runtime_error("Entry for character class " + classname + " was not found.");
	}

	const auto& job = iter->second;
	std::cout << job.mbs.dir << '\t' << job.mbs.path << '\n';

	std::vector<char> buffer;
	if (auto entry = _cpkt.find_file(job.mbs.dir, job.mbs.path); entry == _cpkt.end()) {
		throw std::runtime_error("MBS for character class " + classname + " was not found.");
	} else {
		_cpkt.extract(*entry, buffer);
	}

	auto mbs = MBS::From(buffer);
	auto ftx = std::vector<FTX::Entry>();
	auto flags = iter->second.variants.at(charaname);
	if (auto entry = _cpkt.find_file(job.ftx.dir, job.ftx.path); entry == _cpkt.end()) {
		throw std::runtime_error("FTX for character class " + classname + " was not found.");
	} else {
		_cpkt.extract(*entry, buffer);
		auto txt = FTX::parse(buffer);
		std::move(txt.begin(), txt.end(), std::back_inserter(ftx));
	}
	_sprites.emplace_back(Sprite(std::move(mbs), std::move(ftx), flags, trackid));
}

bool Scene::handle_inputs() {
	SDL_Event event{};
	bool done = false;
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
			switch (event.key.key) {
			case SDLK_DOWN: --_sprites[0]; break;
			case SDLK_UP:   ++_sprites[0]; break;
			}
			break;
		}
		}
	}
	return done;
}

void Scene::render() {
	for (auto& sprite : _sprites) {
		sprite.render(_camera, _projection);
	}
}

void Scene::update(unsigned dt) {
	for (auto& sprite : _sprites) {
		sprite.update(dt);
	}
}
