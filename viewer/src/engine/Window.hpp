#pragma once

#include <functional>
#include <SDL3/SDL.h>
#include <memory>

namespace uvw {

class Window {
public:
	Window(const char* name, int width = 1920, int height = 1080);
	~Window() noexcept;

	SDL_Window* get() const noexcept;
	SDL_WindowFlags flags() const noexcept;
	SDL_WindowID id() const noexcept;

	void clear() const noexcept;
	bool swapbuffer() const noexcept;

private:
	std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> _window;
	std::unique_ptr<SDL_GLContextState, decltype(&SDL_GL_DestroyContext)> _gl;
	long long _tick;
};

std::unique_ptr<char, void(*)(char*)> Init_SDL(int flags = SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

} // namespace uvw