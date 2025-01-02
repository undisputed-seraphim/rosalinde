#include "camera.hpp"
#include "shader.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <boost/program_options.hpp>
#include <eltolinde.hpp>
#include <filesystem>
#include <format>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glxx/buffers.hpp>
#include <glxx/error.hpp>

#include <cstdio>
#include <spanstream>

namespace fs = std::filesystem;
namespace po = ::boost::program_options;

static glm::vec2 interpolated(glm::vec2 from, glm::vec2 to, float interpolation) {
	if (from == to) {
		return from;
	}
	return from + ((to - from) * interpolation);
}

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

GLuint generate_array_texture(const std::vector<FTX::Entry>& textures) {
	float max_x = 0, max_y = 0;
	for (const auto& t : textures) {
		max_x = std::max(max_x, float(t.width));
		max_y = std::max(max_y, float(t.height));
	}
	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D_ARRAY, id);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_ALWAYS);

	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, max_x, max_y, textures.size());
	for (int i = 0; i < textures.size(); ++i) {
		const auto& t = textures[i];
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, t.width, t.height, 1, GL_RGBA, GL_UNSIGNED_BYTE, t.rgba.data());
	}
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	return id;
}

int main(int argc, char* argv[]) try {
	fs::path cpkpath;
	bool debug = false;
	bool list = false;
	int index = 0;
	po::options_description desc;
	desc.add_options()("help,h", "Print this help message")(
		"cpk", po::value<fs::path>(&cpkpath)->required(), "Path to Unicorn.cpk")(
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
	std::cout << cpkpath << std::endl;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		printf("Error: SDL_Init(): %s\n", SDL_GetError());
		return -1;
	}

	const CPK cpk(cpkpath);

	std::vector<char> buffer;
	const auto scarlet_mbs = [&cpk, &buffer]() {
		auto mbs = cpk.by_name("Chara/Scarlet_F.mbs");
		cpk.extract(*mbs, buffer);
		std::cout << "MBS: " << mbs->path() << '\n';
		return MBS(std::ispanstream(buffer, std::ios::binary));
	}();
	const auto scarlet_quad = scarlet_mbs.extract();
	if (list) {
		int i = 0;
		for (const auto& [name, anims] : scarlet_quad.animationsets()) {
			std::cout << i++ << ": " << name << '\n';
		}
		return 0;
	}

	static constexpr int W = 1920, H = 1080;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_Window* window = SDL_CreateWindow("Rosalinde", W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
	if (window == nullptr) {
		printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
		return -1;
	}
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);

	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
		throw std::runtime_error("Failed to initialize GLAD");
	}
	if (debug) {
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(&message_callback, NULL);
	}

	printf(" Version: %s\n", glGetString(GL_VERSION));
	printf("  Vendor: %s\n", glGetString(GL_VENDOR));
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf(" Shading: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	SDL_GL_MakeCurrent(window, gl_context);
	SDL_GL_SetSwapInterval(1); // Enable vsync
	SDL_ShowWindow(window);

	cpk.extract(*cpk.by_name("Chara/Scarlet_F00.ftx"), buffer);
	auto scarlet_textures = FTX::parse(buffer);
	for (auto& texture : scarlet_textures) {
		FTX::decompress(texture);
		FTX::deswizzle(texture);
		std::cout << texture.name << " : " << texture.width << " x " << texture.height << '\n';
	}
	GLuint tex = generate_array_texture(scarlet_textures);
	glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
	glActiveTexture(GL_TEXTURE0);

	const auto& IDLE = *std::next(scarlet_quad.animationsets().begin(), index);
	std::cout << IDLE.first << std::endl;

	gl::uiElementBuffer indices;
	unsigned int VBO;
	glGenBuffers(1, &VBO);
	unsigned int VAO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	enable_blend(glm::vec4(1.0, 1.0, 1.0, 1.0));

	static constexpr unsigned SCARLET_1 = 0x4000 + 0x1000;
	static constexpr unsigned SCARLET_2 = 0x2000 + 0x800;

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
	std::vector<vertex> vertices;

	// Our state
	const auto clear_color = glm::vec4(0.45f, 0.55f, 0.60f, 1.00f);
	Camera cam;
	bool mouseDown = false;
	std::vector<int> trackstep(IDLE.second.size(), 0);
	std::vector<int> tracktime(IDLE.second.size(), 0);

	const auto& shader = GetKeyframeShader().Use();

	for (bool done = false; !done;) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			// ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT)
				done = true;
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
				done = true;
			if (event.type == SDL_EVENT_MOUSE_WHEEL) {
				cam.zoom(event.wheel.y);
			}
			if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
				mouseDown = true;
			}
			if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
				mouseDown = false;
			}
			if (mouseDown && SDL_EVENT_MOUSE_MOTION) {
				cam.move(event.motion.xrel, event.motion.yrel);
			}
		}
		if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
			SDL_Delay(10);
			continue;
		}

		// Rendering
		glViewport(0, 0, W, H);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);

		for (int t = 0; t < IDLE.second.size(); ++t) {
			const auto& track = IDLE.second[t];
			const auto& tl = track.keyframes[tracktime[t]];
			if (trackstep[t]-- == 0) {
				trackstep[t] = tl.frames;
				(++tracktime[t]) %= track.keyframes.size();
			}
			const auto& kf1 = scarlet_quad.keyframes()[tl.keyframe_id];
			if (kf1.layers.empty()) {
				continue;
			}

			const auto& tl2 = (tracktime[t] == track.keyframes.size() - 1) ? track.keyframes[0] : track.keyframes[tracktime[t] + 1];
			const auto& kf2 = scarlet_quad.keyframes()[tl2.keyframe_id];

			vertices.clear();
			indices.storage().clear();

			const float interpolation = float(tl.kf_interpolation) * (1.0 - (float(trackstep[t]) / float(tl.frames)));
			const float zrate = 1.0 / (kf1.layers.size() + 1);
			float depth = 1.0;
			unsigned i = 0;
			for (int k = 0; k < kf1.layers.size(); ++k) {
				const auto& layer1 = scarlet_quad.layers()[kf1.layers[k]];
				const auto& layer2 = kf2.layers.size() < k ? scarlet_quad.layers()[kf2.layers[k]] : layer1;
				if (layer1.attributes & SCARLET_2) {
					continue;
				}
				const auto& tex = scarlet_textures[layer1.texid];
				const auto texdim = glm::vec2{tex.width, tex.height};

				for (int j = 0; j < 4; ++j) {
					vertices.emplace_back(vertex{
						layer1.texid,
						layer1.src[j] * texdim,
						//glm::vec3{layer1.dst[j], depth},
						glm::vec3(interpolated(layer1.dst[j], layer2.dst[j], interpolation), depth),
						layer1.fog[j]
					});
				}

				depth -= zrate;
				indices.storage().insert(indices.storage().end(), {i + 0, i + 1, i + 3, i + 1, i + 2, i + 3});
				i += 4;
			}

			shader.SetUniform("u_mvp", proj * cam.lookAt() * tl.matrix);
			shader.SetUniform("u_tex", 0);
			glBindBuffer(GL_ARRAY_BUFFER, VBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 1, GL_SHORT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, texid));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, uv));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)offsetof(vertex, xyz));
			glEnableVertexAttribArray(3);
			glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void*)offsetof(vertex, fog));

			indices.bind().setData(gl::buffer::Usage::STATIC_DRAW).drawElements(gl::Mode::TRIANGLES);
		}

		SDL_GL_SwapWindow(window);
	}

	// Cleanup
	SDL_GL_DestroyContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();
} catch (const std::exception& e) {
	std::cout << e.what() << std::endl;
	return 1;
}