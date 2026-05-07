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
	bool debug,
	std::string screenshot_path,
	std::string bg_name)
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
	std::cout << job.mbs.dir << '\t' << job.mbs.path << '\n';

	std::vector<char> mbs_buf, buf;
	if (auto entry = _cpkt.find_file(job.mbs.dir, job.mbs.path); entry == _cpkt.end()) {
		throw std::runtime_error("MBS for character class " + classname + " was not found.");
	} else {
		_cpkt.extract(*entry, mbs_buf);
	}

	auto flags = iter->second.variants.at(charaname);
	std::vector<FTX::Entry> ftx_entries;
	if (auto entry = _cpkt.find_file(job.ftx.dir, job.ftx.path); entry == _cpkt.end()) {
		throw std::runtime_error("FTX for character class " + classname + " was not found.");
	} else {
		_cpkt.extract(*entry, buf);
		auto txt = FTX::parse(buf);
		std::move(txt.begin(), txt.end(), std::back_inserter(ftx_entries));
	}

	for (auto& t : ftx_entries) {
		FTX::decompress(t);
		FTX::deswizzle(t);
	}

	_data = SpriteData::load(
		std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(mbs_buf.data()), mbs_buf.size()),
		std::move(ftx_entries));

	_renderer.upload_textures(_data);
	_instance = SpriteInstance{&_data, trackid, flags};
	_instance.play(trackid);
	_camera.fit_bounds(_instance.track_bounds());

	if (!bg_name.empty()) {
		const auto bg_iter = BattleBGs.find(bg_name);
		if (bg_iter == BattleBGs.end()) {
			throw std::runtime_error("Battle BG " + bg_name + " was not found.");
		}
		const auto& bg = bg_iter->second;

		std::vector<char> bg_mbs_buf, bg_ftx_buf;
		if (auto entry = _cpkt.find_file(bg.mbs.dir, bg.mbs.path); entry == _cpkt.end()) {
			throw std::runtime_error("MBS for BG " + bg_name + " was not found.");
		} else {
			_cpkt.extract(*entry, bg_mbs_buf);
		}
		if (auto entry = _cpkt.find_file(bg.ftx.dir, bg.ftx.path); entry == _cpkt.end()) {
			throw std::runtime_error("FTX for BG " + bg_name + " was not found.");
		} else {
			_cpkt.extract(*entry, bg_ftx_buf);
		}
		auto bg_txt = FTX::parse(bg_ftx_buf);
		std::vector<FTX::Entry> bg_ftx_entries;
		std::move(bg_txt.begin(), bg_txt.end(), std::back_inserter(bg_ftx_entries));
		for (auto& t : bg_ftx_entries) {
			FTX::decompress(t);
			FTX::deswizzle(t);
		}

		_bg_data = SpriteData::load(
			std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bg_mbs_buf.data()), bg_mbs_buf.size()),
			std::move(bg_ftx_entries));
		_bg_renderer.upload_textures(_bg_data);
		_bg_instance = SpriteInstance{&_bg_data, 0, 0};
		_bg_instance.play(0);
		_has_bg = true;
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
			if (shift && _has_bg) {
				switch (event.key.key) {
				case SDLK_DOWN: _bg_instance.prev_track(); break;
				case SDLK_UP:   _bg_instance.next_track(); break;
				}
			} else {
				switch (event.key.key) {
				case SDLK_DOWN: _instance.prev_track(); _camera.fit_bounds(_instance.track_bounds()); break;
				case SDLK_UP:   _instance.next_track(); _camera.fit_bounds(_instance.track_bounds()); break;
				}
			}
			break;
		}
		}
	}
	return done;
}

void Scene::render() {
	if (_has_bg) _bg_renderer.draw(_bg_instance, _projection, _camera);
	_renderer.draw(_instance, _projection, _camera);

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
	if (_has_bg) _bg_instance.update(dt);
	_instance.update(dt);
}
