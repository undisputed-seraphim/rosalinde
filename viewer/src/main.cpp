#include "camera.hpp"
#include "engine/Window.hpp"
#include "shader.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <boost/program_options.hpp>
#include <cstdio>
#include <eltolinde.hpp>
#include <filesystem>
#include <format>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glxx/buffers.hpp>
#include <glxx/error.hpp>
#include <span>
#include <spanstream>

namespace fs = std::filesystem;
namespace po = ::boost::program_options;

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

int main(int argc, char* argv[]) try {
	fs::path cpkpath;
	bool debug = false;
	bool list = false;
	int index = 0;
	std::string classname, charaname;
	po::options_description desc;
	desc.add_options()("help,h", "Print this help message")(
		"cpk", po::value<fs::path>(&cpkpath)->required(), "Path to Unicorn.cpk")(
		"class", po::value<std::string>(&classname), "Classname")(
		"chara", po::value<std::string>(&charaname), "Character name")(
		"dbg,d", po::value<bool>(&debug), "Debug messages in OpenGL")(
		"index,i", po::value<int>(&index), "Multipurpose index")("list,l", po::bool_switch(&list), "List animations.");
	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	try {
		po::notify(vm);
		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return 1;
		}
	} catch (const po::required_option& e) {
		std::cout << desc << '\n';
		throw;
	}

	static constexpr int W = 1920, H = 1080;
	const uvw::Window window("Rosalinde", W, H);

	if (debug) {
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(&message_callback, NULL);
	}

	printf(" Version: %s\n", glGetString(GL_VERSION));
	printf("  Vendor: %s\n", glGetString(GL_VENDOR));
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf(" Shading: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	if (classname.empty()) {
		classname = "HighPriestess";
	}
	if (charaname.empty()) {
		charaname = "Scarlett";
	}

	State state(cpkpath);
	state.loadSprite(classname, charaname, index);

	enable_blend(glm::vec4(1.0, 1.0, 1.0, 1.0));

	////////////////

	const auto proj = glm::ortho((-W) / 2.0f, W / 2.0f, H / 2.0f, (-H) / 2.0f);

#pragma pack(push, 1)
	struct vertex {
		int16_t texid;
		glm::vec2 uv;
		glm::vec3 xyz;
		uint32_t fog;
	};
#pragma pack(pop)
	gl::ArrayBuffer<vertex> vertices;
	gl::uiElementBuffer indices;

	unsigned int VAO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	// Our state
	const auto clear_color = glm::vec4(0.45f, 0.55f, 0.60f, 1.00f);
	Camera cam(2.5);

	for (bool done = false; !done;) {
		SDL_Event event{};
		while (SDL_PollEvent(&event)) {
			// ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT) {
				done = true;
			}
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == window.id()) {
				done = true;
			}

			cam.handleInput(event);
		}
		if (window.flags() & SDL_WINDOW_MINIMIZED) {
			SDL_Delay(10);
			continue;
		}

		// Rendering
		glViewport(0, 0, W, H);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		// window.clear();

		//SDL_Delay(40); // Slow down animation

		state.render(cam, proj);

		window.swapbuffer();
	}
} catch (const std::exception& e) {
	std::cout << e.what() << std::endl;
	return 1;
}