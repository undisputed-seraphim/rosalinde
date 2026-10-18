#pragma once

#include <SDL3/SDL.h>
#include <array>
#include <functional>
#include <memory>

namespace uvw {

class Window {
public:
	Window(const char* name, int width = 1920, int height = 1080);
	Window(const Window&) = delete;
	~Window() noexcept;

	SDL_Window* get() const noexcept;
	SDL_WindowFlags flags() const noexcept;
	SDL_WindowID id() const noexcept;

	void set_clear_color(const std::array<float, 4>& color) noexcept { _clear_color = color; }
	void clear() const noexcept;
	bool swapbuffer() const noexcept;

private:
	std::unique_ptr<char, void (*)(char*)> _sdl;
	std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> _window;
	std::unique_ptr<SDL_GLContextState, decltype(&SDL_GL_DestroyContext)> _gl;
	std::array<float, 4> _clear_color = {0.45f, 0.55f, 0.60f, 1.0f};
	long long _tick;
};

} // namespace uvw