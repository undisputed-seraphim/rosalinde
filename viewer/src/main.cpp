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

enum s4flag : uint8_t {
	SKIP = 0x02,
	NOTEX = 0x04,
};

enum s8flag : uint32_t {
	// clang-format off
	FLIPX = 0x01,   // 0b ----'----'----'---1
	FLIPY = 0x02,   // 0b ----'----'----'--1-
	JUMP = 0x04,    // 0b ----'----'----'-1-- // Something to do with the loops?
	//? = 0x20,     // 0b ----'----'--1-'---- // Most s8 seem to have this, but doesn't seem to mean anything
	//? = 0x80,     // 0b ----'----'1---'---- // doesn't mean anything
	HITBOX = 0x400, // 0b ----'-1--'----'----
	LAST = 0x800,	// 0b ----'1---'----'---- // Last frame / Loop to beginning
	//? = 0x2000,   // 0b --1-'----'----'---- // Ignore next?
	// clang-format on
};

static glm::mat4 s7_matrix(const mbs::section_7& s7, const bool flipx, const bool flipy) {
	const int8_t x = flipx ? -1 : 1;
	const int8_t y = flipy ? -1 : 1;
	glm::mat4 m{1.0};
	m = glm::scale(m, glm::vec3{s7.scale.x * x, s7.scale.y * y, 1.0});
	m = glm::translate(m, glm::vec3{s7.move.x * x, s7.move.y * y, s7.move.z});
	m *= glm::eulerAngleXYZ(s7.rotate.x, s7.rotate.y, s7.rotate.z);
	// NOTE: Normally the right order for this is scale-rotate-translate,
	// however accessories seem to be wrongly placed.
	// So scale-translate-rotate appears to get us closest to the right image.
	// I think there is some parent-child transform hierarchy that's currently missing.
	return m;
}

void print_animation_list(const State::Sprite& sprite) {
	int i = 0;
	std::cout << sprite.mbs.filename() << '\n';
	for (const auto& s9 : sprite.mbs.get().s9) {
		if (s9.name[0] == '\0') continue;
		std::cout << s9.name << '\n';
	}
	std::cout << std::endl;
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
		"index,i", po::value<int>(&index), "Multipurpose index")(
		"list,l", po::bool_switch(&list), "List animations.");
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

	if (classname.empty()) { classname = "HighPriestess"; }
	if (charaname.empty()) { charaname = "Scarlett"; }

	State state(cpkpath.string());
	const auto sprite = state.FetchCharacterSprite(classname, charaname);
	const auto bg = state.FetchBackgroundSprite("BGBtVillage_a");
	if (list) {
		print_animation_list(sprite);
		print_animation_list(bg);
		return 0;
	}

	const auto& char_v77 = sprite.mbs.get();
	const auto& s9 = char_v77.s9[index];
	std::cout << s9.name << std::endl;

	const auto& bg_v77 = bg.mbs.get();

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
	const auto& shader = GetKeyframeShader().Use();

	uint32_t timestep = 0;
	for (bool done = false; !done; ) {
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

		SDL_Delay(40); // Slow down animation

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, bg.glTexHandle);

		for (const auto& s9 : bg_v77.s9) {
			const auto saspan = std::span(bg_v77.sa.begin() + s9.sa_set_id, s9.sa_set_no);
			for (const auto& sa : saspan) {
				const auto& s8 = bg_v77.s8[sa.s8_id + (timestep % sa.s8_no)];
				const auto& s6 = bg_v77.s6[s8.s6_id];

				if (s8.flags & s8flag::HITBOX) {
					continue;
				}

				if (s6.s4_no == 0) {
					continue;
				}

				vertices.storage().clear();
				indices.storage().clear();

				const float zrate = 1.0 / (s6.s4_no + 1);
				float depth = 1.0;
				uint32_t l = 0;
				for (uint32_t j = 0; j < s6.s4_no; ++j) {
					const auto& s4 = bg_v77.s4[s6.s4_id + j];
					const auto& tex = bg.textures[s4.tex_id];
					const auto texdim = glm::vec2{tex.width, tex.height};
					for (int k = 0; k < 4; ++k) {
						const auto& dst = bg_v77.s2[s4.s2_id].values[k];
						const auto& src = bg_v77.s1[s4.s1_id].values[k];
						const auto& fog = bg_v77.s0[s4.s0_id].colors[k];
						vertices.storage().emplace_back(
							vertex{s4.tex_id, src * texdim, glm::vec3{dst, depth}, fog});
					}
					depth -= zrate;
					indices.storage().insert(indices.storage().end(), {l + 0, l + 1, l + 3, l + 1, l + 2, l + 3});
					l += 4;
				}

				const bool flipx = s8flag::FLIPX & s8.flags;
				const bool flipy = s8flag::FLIPY & s8.flags;
				const auto& s7 = bg_v77.s7[s8.s7_id];
				const auto s7m = s7_matrix(s7, flipx, flipy);

				shader.SetUniform("u_mvp", proj * cam.lookAt() * s7m);
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
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, sprite.glTexHandle);

		for (uint16_t i = 0; i < s9.sa_set_no; ++i) { // For each track
			const auto& sa = char_v77.sa[s9.sa_set_id + i];
			const auto& s8 = char_v77.s8[sa.s8_id + (timestep % (sa.s8_no))];
			const auto& s7 = char_v77.s7[s8.s7_id]; // Matrix
			const auto& s6 = char_v77.s6[s8.s6_id]; // Keyframe
			//const auto& s5 = char_v77.s5[s6.s5_id]; // Hitbox
			//const auto& s3 = char_v77.s3[s5.s3_id]; // Hitbox Matrix

			if (s8.flags & s8flag::HITBOX) { // Don't care about hitbox for now
				continue;
			}

			if (s6.s4_no == 0) { // Ignore if frame does not have any content in it
				continue;
			}

			vertices.storage().clear();
			indices.storage().clear();

			// Draw each layer
			const float zrate = 1.0f / (s6.s4_no + 1);
			float depth = 1.0;
			uint32_t l = 0;
			for (uint32_t j = 0; j < s6.s4_no; ++j) { // For each layer
				const auto& s4 = char_v77.s4[s6.s4_id + j];
				if ((s4.attributes & ~sprite.flags) != 0) {
					continue;
				}

				const auto& tex = sprite.textures[s4.tex_id];
				const auto texdim = glm::vec2{tex.width, tex.height};
				for (int k = 0; k < 4; ++k) {
					const auto& dst = char_v77.s2[s4.s2_id].values[k];
					const auto& src = char_v77.s1[s4.s1_id].values[k];
					const auto& fog = char_v77.s0[s4.s0_id].colors[k];
					vertices.storage().emplace_back(
						vertex{s4.tex_id, src * texdim, glm::vec3{dst, depth}, fog});
				}
				depth -= zrate;
				indices.storage().insert(indices.storage().end(), {l + 0, l + 1, l + 3, l + 1, l + 2, l + 3});
				l += 4;
			}

			const bool flipx = s8flag::FLIPX & s8.flags;
			const bool flipy = s8flag::FLIPY & s8.flags;
			const auto s7m = s7_matrix(s7, flipx, flipy);

			shader.SetUniform("u_mvp", proj * cam.lookAt() * s7m);
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

		++timestep;

		window.swapbuffer();
	}
} catch (const std::exception& e) {
	std::cout << e.what() << std::endl;
	return 1;
}