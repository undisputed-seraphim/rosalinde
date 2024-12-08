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
#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/gtx/string_cast.hpp>

#include <cstdio>
#include <spanstream>

namespace fs = std::filesystem;
namespace po = ::boost::program_options;

auto camera = glm::mat4{2, 0, 0, 0, 0, 2, 0, 322, 0, 0, 1, 0, 0, 0, 0, 1};

// IN:  Quad vertices
// OUT: Triangle vertices, transformed
glm::mat4x3 transform(glm::mat4x2 vertices) {
	// clang-format off
	camera[0][0] = 2.07219708396179;
	camera[1][1] = 2.07219708396179;

	const auto dst = glm::mat4x2{
		camera[0][0] * vertices[0][0] + camera[0][1] * vertices[0][1] + camera[0][2] + camera[0][3],
		camera[1][0] * vertices[0][0] + camera[1][1] * vertices[0][1] + camera[1][2] + camera[1][3],
	
		camera[0][0] * vertices[1][0] + camera[0][1] * vertices[1][1] + camera[0][2] + camera[0][3],
		camera[1][0] * vertices[1][0] + camera[1][1] * vertices[1][1] + camera[1][2] + camera[1][3],

		camera[0][0] * vertices[2][0] + camera[0][1] * vertices[2][1] + camera[0][2] + camera[0][3],
		camera[1][0] * vertices[2][0] + camera[1][1] * vertices[2][1] + camera[1][2] + camera[1][3],

		camera[0][0] * vertices[3][0] + camera[0][1] * vertices[3][1] + camera[0][2] + camera[0][3],
		camera[1][0] * vertices[3][0] + camera[1][1] * vertices[3][1] + camera[1][2] + camera[1][3],
	};

	// perspective_mat3
	const auto v = glm::mat4x3{
		glm::vec3{dst[0], 1.0},
		glm::vec3{dst[1], 1.0},
		glm::vec3{dst[2], 1.0},
		glm::vec3{dst[3], 1.0}
	};
	const auto c = glm::mat3{
		glm::cross(glm::cross(v[0], v[2]), glm::cross(v[1], v[3])),
		glm::cross(glm::cross(v[0], v[1]), glm::cross(v[3], v[2])),
		glm::cross(glm::cross(v[0], v[3]), glm::cross(v[1], v[2]))
	};
	const auto h = glm::mat3{
		c[0][0], c[1][0], c[2][0],
		c[0][1], c[1][1], c[2][1],
		c[0][2], c[1][2], c[2][2]
	};

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

glm::mat4x2 transform_UV(const Texture2D& texture, glm::mat4x2 quad) {
	for (int i = 0; i < 4; ++i) {
		quad[i][0] = quad[i][0] * texture.Width;
		quad[i][1] = quad[i][1] * texture.Height;
	}
	return quad;
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

int main(int argc, char* argv[]) try {
	fs::path cpkpath;
	po::options_description desc;
	desc.add_options()("help,h", "Print this help message")(
		"cpk", po::value<fs::path>(&cpkpath)->required(), "Path to Unicorn.cpk");
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

	constexpr const char* glsl_version = "#version 300 es";
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
		auto mbs = cpk.by_name("Scarlet_F.mbs", "Chara");
		cpk.extract(*mbs, buffer);
		std::cout << "MBS: " << mbs->name << '\n';
		return MBS(std::ispanstream(buffer, std::ios::binary));
	}();
	const auto scarlet_quad = scarlet_mbs.extract();
	const auto scarlet_textures = [&cpk, &buffer]() {
		auto ftx = cpk.by_name("Scarlet_F00.ftx", "Chara");
		cpk.extract(*ftx, buffer);
		std::vector<Texture2D> ret;
		for (const auto& texentry : FTX::parse(buffer)) {
			std::cout << "Texture: " << texentry.name << '\t' << texentry.width << ':' << texentry.height
					  << " bytes: " << texentry.rgba.size() << '\n';
			ret.emplace_back(texentry.width, texentry.height, texentry.rgba.data());
		}
		return ret;
	}();

	int timestep = 0;

	const auto& IDLE = scarlet_quad.skeletons()[0];
	const Quad::Animation* selected_anim = &scarlet_quad.animations()[IDLE.bones[0].id];

	unsigned int VBO[4];
	unsigned int VAO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(4, VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	////////////////

	std::vector<glm::mat4x3> xyz;
	std::vector<glm::mat4x2> uv;
	std::vector<unsigned> indices;
	std::vector<float> fog;
	std::vector<float> z;
	std::vector<short> texid;

	// Our state
	const auto clear_color = glm::vec4(0.45f, 0.55f, 0.60f, 1.00f);

	bool done = false;
#ifdef __EMSCRIPTEN__
	// For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the
	// imgui.ini file. You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
	io.IniFilename = nullptr;
	EMSCRIPTEN_MAINLOOP_BEGIN
#else
	while (!done)
#endif
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
		glClearColor(
			clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
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

				//const auto& blend = scarlet_quad.blends()[kf.layers[0].blendid];
				enable_blend(glm::vec4(1.0, 1.0, 1.0, 1.0));
				//enable_depth(GL_LESS); // No output with this.

				fog.resize(kf.layers.size() * 16, 1.0);
				const float zrate = 1.0 / (kf.layers.size() + 1);
				float depth = 1.0;
				unsigned i = 0;
				for (const auto& layer : kf.layers) {

					const auto& texture = scarlet_textures[layer.texid];
					uv.push_back(transform_UV(texture, layer.src));
					xyz.push_back(transform(layer.dst));
					indices.insert(indices.end(), {i + 0, i + 1, i + 2, i + 0, i + 2, i + 3});
					texid.insert(texid.end(), {layer.texid, layer.texid, layer.texid, layer.texid});

					depth -= zrate;
					z.insert(z.end(), {depth, depth, depth, depth});
					i += 4;
				}

				// for (int i = 0; i < scarlet_textures.size(); ++i) {
				//	scarlet_textures[i].Active(GL_TEXTURE0 + i).Bind();
				// }
				const auto& texture = scarlet_textures[1].Active(GL_TEXTURE0).Bind();
				// scarlet_textures[1].Active(GL_TEXTURE0).Bind();

				const auto& shader = GetKeyframeShader().Use();
				shader.SetUniform("u_pxsize", {(float)W, (float)H});
				shader.SetUniform("u_imsize", {1.0 / texture.Width, 1.0 / texture.Height});
				shader.SetUniform("u_tex", 0);

				glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
				glBufferData(GL_ARRAY_BUFFER, sizeof(float) * fog.size(), fog.data(), GL_STATIC_DRAW);
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
#ifdef __EMSCRIPTEN__
	EMSCRIPTEN_MAINLOOP_END;
#endif

	// Cleanup
	SDL_GL_DestroyContext(gl_context);
	SDL_DestroyWindow(window);
	SDL_Quit();
} catch (const std::exception& e) {
	std::cout << e.what() << std::endl;
	return 1;
}