#pragma once

#include <glad/glad.h>

namespace gl {

namespace texture {

enum Type2D {
	TEX_2D = GL_TEXTURE_2D,
	TEX_CUBE_MAP_POSITIVE_X = GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	TEX_CUBE_MAP_NEGATIVE_X = GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	TEX_CUBE_MAP_POSITIVE_Y = GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	TEX_CUBE_MAP_NEGATIVE_Y = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	TEX_CUBE_MAP_POSITIVE_Z = GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
	TEX_CUBE_MAP_NEGATIVE_Z = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
};

enum Type3D {
	TEX_3D = GL_TEXTURE_3D,
	TEX_2D_ARRAY = GL_TEXTURE_2D_ARRAY,
};

} // namespace texture

template <texture::Type2D TexType>
class Texture {
private:
	GLuint _hnd = 0;
	unsigned _w = 0, _h = 0;

public:
	static constexpr texture::Type2D TextureType = TexType;

	Texture() noexcept { glGenTextures(1, &_hnd); }
	Texture(const Texture&) = delete;
	Texture(Texture&& o) noexcept
		: _hnd(o._hnd)
		, _w(o._w)
		, _h(o._h) {
		o._hnd = 0;
		o._w = 0;
		o._h = 0;
	}
	~Texture() noexcept { glDeleteTextures(1, &_hnd); }

	Texture& Init() noexcept { return *this; }

	Texture& Active(GLenum activeid) noexcept {
		glActiveTexture(activeid);
		return *this;
	}
	const Texture& Active(GLenum activeid) const noexcept {
		glActiveTexture(activeid);
		return *this;
	}
	Texture& Bind() noexcept {
		glBindTexture(TextureType, _hnd);
		return *this;
	}
	const Texture& Bind() const noexcept {
		glBindTexture(TextureType, _hnd);
		return *this;
	}

	unsigned width() const noexcept { return _w; }
	unsigned height() const noexcept { return _h; }

	explicit operator GLuint() const noexcept { return _hnd; }
	explicit operator bool() const noexcept { return (_hnd != 0) && (glIsTexture(_hnd)); }

	Texture& operator=(const Texture& other) = delete;
	Texture& operator=(Texture&& other) = default;
};

using Texture2D = Texture<texture::Type2D::TEX_2D>;

} // namespace gl