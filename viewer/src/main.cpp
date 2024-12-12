#include "shader.hpp"
#include "state.hpp"
#include "texture.hpp"

#include <SDL3/SDL.h>
#include <array>
#include <boost/program_options.hpp>
#include <eltolinde.hpp>
#include <filesystem>
#include <format>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glxx/error.hpp>

#include <cstdio>
#include <spanstream>

namespace fs = std::filesystem;
namespace po = ::boost::program_options;

// clang-format off
static auto camera = glm::mat4{
	2, 0, 0, 0,
	0, 2, 0, 322,
	0, 0, 1, 0,
	0, 0, 0, 1
};
// clang-format on

// IN:  Quad vertices
// OUT: Triangle vertices, transformed
static glm::mat4x3 transform(glm::mat4x2 vertices) {
	// clang-format off
	camera[0][0] = 2.07219708396179;
	camera[1][1] = 2.07219708396179;

	const auto v2 = glm::mat4{
		glm::vec4(vertices[0], 1.0, 1.0),
		glm::vec4(vertices[1], 1.0, 1.0),
		glm::vec4(vertices[2], 1.0, 1.0),
		glm::vec4(vertices[3], 1.0, 1.0),
	};

	const glm::mat4x3 v = glm::transpose(camera) * v2;
	const auto h = glm::transpose(glm::mat3{
		glm::cross(glm::cross(v[0], v[2]), glm::cross(v[1], v[3])),
		glm::cross(glm::cross(v[0], v[1]), glm::cross(v[3], v[2])),
		glm::cross(glm::cross(v[0], v[3]), glm::cross(v[1], v[2]))
	});

	//perspective_quad
	const auto h_inv = glm::mat3{
		 0,     0,  0.005,
	-0.001,     0,  0.015,
		 0, 0.001, -0.015
	};
	const glm::mat3 m3 = h_inv * h;
	const auto t = glm::mat4x3{
		glm::vec3{10, 10, 1} * m3,
		glm::vec3{20, 10, 1} * m3,
		glm::vec3{20, 20, 1} * m3,
		glm::vec3{10, 20, 1} * m3,
	};
	// clang-format on
	return t;
}

static glm::mat4x2 transformUV(glm::mat4x2 uv, const std::vector<FTX::Entry>& textures, int16_t texid) {
	const auto& t = textures[texid];
	const glm::vec2 dims{t.width, t.height};
	glm::vec2 d(0.0);
	for (int16_t i = 0; i < textures.size(); ++i) {
		if (i == texid) {
			for (int j = 0; j < 4; ++j) {
				uv[j] *= dims;
				uv[j][0] += d[0];

				// Due to some weird error, the x-axis of the texture
				// must be shifted right by 2, to avoid artifacts.
				uv[j][0] += 2;
			}
		}
		d[0] += float(textures[i].width);
		d[1] = std::max(d[1], float(textures[i].height));
	}
	uv /= d;
	return uv;
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

GLuint generate_texture_map(const std::vector<FTX::Entry>& textures) {
	unsigned total_width = 0;
	unsigned max_height = 0;
	for (const auto& t : textures) {
		total_width += t.width;
		max_height = std::max(max_height, (unsigned)t.height);
	}

	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, total_width, max_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	total_width = 0;
	for (const auto& t : textures) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, total_width, 0, t.width, t.height, GL_RGBA, GL_UNSIGNED_BYTE, t.rgba.data());
		total_width += t.width;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindTexture(GL_TEXTURE_2D, 0);
	return id;
}

int main(int argc, char* argv[]) try {
	fs::path cpkpath;
	bool debug = false;
	int index = 0;
	po::options_description desc;
	desc.add_options()("help,h", "Print this help message")(
		"cpk", po::value<fs::path>(&cpkpath)->required(), "Path to Unicorn.cpk")(
		"dbg,d", po::value<bool>(&debug), "Debug messages in OpenGL")(
		"index,i", po::value<int>(&index), "Multipurpose index");
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

	const CPK cpk(cpkpath.string());

	std::vector<char> buffer;
	const auto scarlet_mbs = [&cpk, &buffer]() {
		auto mbs = cpk.by_name("Virginia_F.mbs", "Chara");
		cpk.extract(*mbs, buffer);
		std::cout << "MBS: " << mbs->name << '\n';
		return MBS(std::ispanstream(buffer, std::ios::binary));
	}();
	const auto scarlet_quad = scarlet_mbs.extract();
	cpk.extract(*cpk.by_name("Virginia_F00.ftx", "Chara"), buffer);
	const auto scarlet_textures = FTX::parse(buffer);
	GLuint tex = generate_texture_map(scarlet_textures);
	glBindTexture(GL_TEXTURE_2D, tex);
	glActiveTexture(GL_TEXTURE0);

	int timestep = 0;

	const auto& IDLE = scarlet_quad.skeletons()[index];
	const Quad::Animation* selected_anim = &scarlet_quad.animations()[IDLE.bones[0].id];

	unsigned int VBO[5];
	unsigned int VAO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(5, VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	enable_blend(glm::vec4(1.0, 1.0, 1.0, 1.0));

	static constexpr unsigned SCARLET_1 = 0x4000 + 0x1000;
	static constexpr unsigned SCARLET_2 = 0x2000 + 0x800;

	////////////////

	std::vector<glm::mat4x3> xyz;
	std::vector<glm::mat4x2> uv;
	std::vector<unsigned> indices;
	std::vector<glm::vec4> fog;
	std::vector<float> z;

	// Our state
	const auto clear_color = glm::vec4(0.45f, 0.55f, 0.60f, 1.00f);

	bool done = false;
	while (!done)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			// ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT)
				done = true;
			if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
				done = true;
		}
		if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
			SDL_Delay(10);
			continue;
		}

		// Rendering
		glViewport(0, 0, W, H);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);

		SDL_Delay(50); // Slow down animation
		if (selected_anim != nullptr) {
			const auto& tl = selected_anim->timelines[timestep++];
			timestep %= selected_anim->timelines.size();
			if (tl.attach.objt == Quad::ObjectType::KEYFRAME) {
				xyz.clear();
				uv.clear();
				fog.clear();
				z.clear();
				indices.clear();

				const auto& kf = scarlet_quad.keyframes()[tl.attach.id];

				const float zrate = 1.0 / (kf.layers.size() + 1);
				float depth = 1.0;
				unsigned i = 0;
				for (const auto& layer : kf.layers) {
					if (layer.attribute & 0x8000) {
						continue;
					}

					xyz.push_back(transform(layer.dst));
					const auto& texture = scarlet_textures[layer.texid];
					uv.push_back(transformUV(layer.src, scarlet_textures, layer.texid));
					fog.insert(fog.end(), layer.fog.begin(), layer.fog.end());

					depth -= zrate;
					z.insert(z.end(), {depth, depth, depth, depth});
					indices.insert(indices.end(), {i + 0, i + 1, i + 2, i + 0, i + 2, i + 3});
					i += 4;
				}

				const auto& shader = GetKeyframeShader().Use();

				shader.SetUniform("u_pxsize", {(float)W, (float)H});
				shader.SetUniform("u_tex", 0);

				glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
				glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * fog.size(), &fog[0][0], GL_STATIC_DRAW);
				glEnableVertexAttribArray(0);
				glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);

				glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
				glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4x3) * xyz.size(), &xyz[0][0][0], GL_STATIC_DRAW);
				glEnableVertexAttribArray(1);
				glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

				glBindBuffer(GL_ARRAY_BUFFER, VBO[2]);
				glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4x2) * uv.size(), &uv[0][0][0], GL_STATIC_DRAW);
				glEnableVertexAttribArray(2);
				glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

				glBindBuffer(GL_ARRAY_BUFFER, VBO[3]);
				glBufferData(GL_ARRAY_BUFFER, sizeof(float) * z.size(), z.data(), GL_STATIC_DRAW);
				glEnableVertexAttribArray(3);
				glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);

				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
				glBufferData(
					GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned) * indices.size(), indices.data(), GL_STATIC_DRAW);

				glDrawElements(GL_TRIANGLES, xyz.size() * 4 * 3, GL_UNSIGNED_INT, 0);
			}
			if (tl.attach.objt == Quad::ObjectType::ANIMATION) {
				const auto& anim = scarlet_quad.animations().at(tl.attach.id);
			}
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