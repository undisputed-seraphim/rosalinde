#include "camera.hpp"
#include "engine/Window.hpp"
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

	const auto sdl = uvw::Init_SDL();

	static constexpr int W = 1920, H = 1080;
	uvw::Window window("Rosalinde", W, H);

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

	int timestep = 0;

	const auto& IDLE = *std::next(scarlet_quad.animationsets().begin(), index);
	std::cout << IDLE.first << std::endl;

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
	gl::ArrayBuffer<vertex> vertices;
	gl::uiElementBuffer indices;

	unsigned int VAO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	// Our state
	const auto clear_color = glm::vec4(0.45f, 0.55f, 0.60f, 1.00f);
	Camera cam;
	const auto& shader = GetKeyframeShader().Use();

	bool done = false;
	while (!done) {
		SDL_Event event{};
		while (SDL_PollEvent(&event)) {
			// ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT)
				done = true;
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == window.id())
				done = true;
			if (event.type == SDL_EVENT_MOUSE_WHEEL) {
				cam.zoom(event.wheel.y);
			}
			if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
				cam.enter();
			}
			if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
				cam.exit();
			}
			if (event.type == SDL_EVENT_MOUSE_MOTION) {
				cam.move(event.motion.xrel, event.motion.yrel);
			}
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

		SDL_Delay(50); // Slow down animation

		int longest_track_size = 0;
		for (const auto& track : IDLE.second) {
			const auto& tl = track.keyframes[timestep % track.keyframes.size()];
			const auto& kf = scarlet_quad.keyframes()[tl.keyframe_id];
			if (kf.layers.empty()) {
				continue;
			}
			longest_track_size = std::max(longest_track_size, (int)track.keyframes.size());
			vertices.storage().clear();
			indices.storage().clear();

			const float zrate = 1.0 / (kf.layers.size() + 1);
			float depth = 1.0;
			unsigned i = 0;
			for (const auto layerid : kf.layers) {
				const auto& layer = scarlet_quad.layers()[layerid];
				if (layer.attributes & SCARLET_2) {
					continue;
				}
				const auto& tex = scarlet_textures[layer.texid];
				const auto texdim = glm::vec2{tex.width, tex.height};

				for (int j = 0; j < 4; ++j) {
					vertices.storage().emplace_back(
						vertex{layer.texid, layer.src[j] * texdim, glm::vec3{layer.dst[j], depth}, layer.fog[j]});
				}

				depth -= zrate;
				indices.storage().insert(indices.storage().end(), {i + 0, i + 1, i + 3, i + 1, i + 2, i + 3});
				i += 4;
			}

			shader.SetUniform("u_mvp", proj * cam.lookAt() * tl.matrix);
			shader.SetUniform("u_tex", 0);
			vertices.bind().setData(gl::buffer::Usage::STATIC_DRAW);
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
		(++timestep) %= longest_track_size;

		window.swapbuffer();
	}
} catch (const std::exception& e) {
	std::cout << e.what() << std::endl;
	return 1;
}