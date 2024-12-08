#pragma once

#include <glad/glad.h>

namespace gl {

class Texture {
private:
	GLuint _hnd = 0;

public:
	Texture();
	Texture(const Texture&) = delete;
	Texture(Texture&& other) noexcept
		: _hnd(other._hnd) {
		other._hnd = 0;
	}
	~Texture() noexcept { glDeleteTextures(1, &_hnd); }

	explicit operator GLuint() const noexcept { return _hnd; }
	explicit operator bool() const noexcept { return (_hnd != 0) && (glIsTexture(_hnd)); }

	Texture& operator=(const Texture& other) = delete;
	Texture& operator=(Texture&& other) = default;
};

} // namespace gl