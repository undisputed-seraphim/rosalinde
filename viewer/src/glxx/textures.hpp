#pragma once

#include <cassert>
#include <glad/glad.h>
#include <span>
#include <stdexcept>
#include <type_traits>

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
	GLuint _hnd;
	unsigned _w, _h;

public:
	static constexpr texture::Type2D TextureType = TexType;
	static constexpr GLenum ValueType = GL_UNSIGNED_BYTE;
	static constexpr GLenum Format = GL_RGBA;

	Texture() noexcept
		: _hnd(0)
		, _w(0)
		, _h(0) {}
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

	Texture& Init(unsigned width, unsigned height, std::span<const uint8_t> data = {}) noexcept {
		if (_hnd == 0) {
			glGenTextures(1, &_hnd);
		}
		_w = width;
		_h = height;
		const auto *const ptr = data.empty() ? NULL : data.data();
		glTexImage2D(TextureType, 0, Format, _w, _h, 0, Format, ValueType, ptr);
		return *this;
	}

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
	Texture& Unbind() noexcept {
		glBindTexture(TextureType, 0);
		return *this;
	}
	const Texture& Unbind() const noexcept {
		glBindTexture(TextureType, 0);
		return *this;
	}

	Texture& SetParameter(GLenum pname, int param) {
		glTexParameteri(TextureType, pname, param);
		return *this;
	}
	const Texture& SetParameter(GLenum pname, int param) const {
		glTexParameteri(TextureType, pname, param);
		return *this;
	}
	Texture& SetParameter(GLenum pname, float param) {
		glTexParameterf(TextureType, pname, param);
		return *this;
	}
	const Texture& SetParameter(GLenum pname, float param) const {
		glTexParameterf(TextureType, pname, param);
		return *this;
	}
	Texture& SetParameter(GLenum pname, std::span<const int> params) {
		glTexParameteriv(TextureType, pname, params.data());
		return *this;
	}
	const Texture& SetParameter(GLenum pname, std::span<const int> params) const {
		glTexParameteriv(TextureType, pname, params.data());
		return *this;
	}
	Texture& SetParameter(GLenum pname, std::span<const float> params) {
		glTexParameterfv(TextureType, pname, params.data());
		return *this;
	}
	const Texture& SetParameter(GLenum pname, std::span<const float> params) const {
		glTexParameterfv(TextureType, pname, params.data());
		return *this;
	}

	unsigned width() const noexcept { return _w; }
	unsigned height() const noexcept { return _h; }

	explicit operator GLuint() const noexcept { return _hnd; }
	explicit operator bool() const noexcept { return (_hnd != 0) && (glIsTexture(_hnd)); }

	Texture& operator=(const Texture& other) = delete;
	Texture& operator=(Texture&& other) = default;
};

template <texture::Type2D TexType>
class TextureAtlas : public Texture<TexType> {
	unsigned _w_occupied, _h_occupied;

	struct subtex_id {

	};
public:
	using Texture<TexType>::Texture;
	using Texture<TexType>::TextureType;
	using Texture<TexType>::ValueType;
	using Texture<TexType>::Format;

	TextureAtlas& SetSubImage(unsigned width, unsigned height, std::span<const uint8_t> data) {
		unsigned xoff = 0, yoff = 0;
		if (_w_occupied + width <= this->width()) {
			xoff = _w_occupied;
			_w_occupied += width;
		} else if (_h_occupied + height <= this->height()) {
			yoff = _h_occupied;
			_h_occupied += height;
		} else {
			throw std::runtime_error("Not enough space in texture atlas");
		}
		glTexSubImage2D(TextureType, 0, xoff, yoff, width, height, Format, ValueType, data.data());
		return *this;
	}
};

using Texture2D = Texture<texture::Type2D::TEX_2D>;
//using TextureAtlas2D = TextureAtlas<texture::Type2D::TEX_2D>;

} // namespace gl