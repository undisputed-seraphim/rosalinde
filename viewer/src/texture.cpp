#include <cassert>
#include <glad/glad.h>
#include <iostream>

#include "texture.hpp"

Texture2D::Texture2D()
	: _hnd(0)
	, _w(0)
	, _h(0) {}

Texture2D::Texture2D(Texture2D&& o) noexcept
	: _hnd(o._hnd)
	, _w(o._w)
	, _h(o._h) {
	o._hnd = 0;
	o._w = 0;
	o._h = 0;
}

Texture2D::~Texture2D() noexcept {
	Unbind();
	glDeleteTextures(1, &_hnd);
}

void Texture2D::Init(const Image image) {
	if (_hnd == 0) {
		glGenTextures(1, &_hnd);
	}
	_w = image.width;
	_h = image.height;
	Bind();
	// glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	Unbind();
}

void Texture2D::Subimage(const Image image, unsigned xoffset, unsigned yoffset) {
	assert(_w >= image.width + xoffset);
	assert(_h >= image.height + yoffset);
	Bind();
	glTexSubImage2D(
		GL_TEXTURE_2D, 0, xoffset, yoffset, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels);
	Unbind();
}

const Texture2D& Texture2D::Active(unsigned samplerID) const noexcept {
	glActiveTexture(samplerID);
	return *this;
}
const Texture2D& Texture2D::Bind() const noexcept {
	glBindTexture(GL_TEXTURE_2D, _hnd);
	return *this;
}
const Texture2D& Texture2D::Unbind() const noexcept {
	glBindTexture(GL_TEXTURE_2D, 0);
	return *this;
}

Texture2D::operator unsigned() const noexcept { return _hnd; }
Texture2D::operator bool() const noexcept { return (_hnd != 0) && (glIsTexture(_hnd) == GL_TRUE); }
