#include "Window.hpp"

#include <array>
#include <glad/glad.h>
#include <stdexcept>

namespace uvw {

static std::unique_ptr<char, void (*)(char*)> Init_SDL(int flags) {
	if (!SDL_Init(flags)) {
		const auto err = std::string("Error: SDL_Init(): ") + SDL_GetError() + '\n';
		throw std::runtime_error(err);
	}
	return std::unique_ptr<char, void (*)(char*)>(new char[0], [](char* ptr) {
		delete ptr;
		SDL_Quit();
	});
}

Window::Window(const char* name, int width, int height)
	: _sdl(Init_SDL(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
	, _window(SDL_CreateWindow(name, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN), SDL_DestroyWindow)
	, _gl(nullptr, SDL_GL_DestroyContext) {
	SDL_SetWindowPosition(_window.get(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetSwapInterval(1); // Enable vsync
	_gl.reset(SDL_GL_CreateContext(_window.get()));
	SDL_GL_MakeCurrent(_window.get(), _gl.get());
	SDL_ShowWindow(_window.get());

	if (!_window) {
		const auto err = std::string("Error: SDL_CreateWindow(): ") + SDL_GetError() + '\n';
		throw std::runtime_error(err);
	}
	if (!_gl) {
		const auto err = std::string("Error: SDL_GL_CreateContext(): ") + SDL_GetError() + '\n';
		throw std::runtime_error(err);
	}
	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
		throw std::runtime_error("Failed to initialize GLAD");
	}
}

Window::~Window() noexcept {}

SDL_Window* Window::get() const noexcept { return _window.get(); }
SDL_WindowID Window::id() const noexcept { return SDL_GetWindowID(_window.get()); }
SDL_WindowFlags Window::flags() const noexcept { return SDL_GetWindowFlags(_window.get()); }

void Window::clear() const noexcept {
	constexpr auto clear_color = std::array<float, 4>{0.45f, 0.55f, 0.60f, 1.0f};
	glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
	glClear(GL_COLOR_BUFFER_BIT);
}
bool Window::swapbuffer() const noexcept { return SDL_GL_SwapWindow(_window.get()); }

} // namespace uvw