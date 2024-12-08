#include <glad/glad.h>
#include <iostream>

#include "texture.hpp"

Texture2D::Texture2D()
	: Width(0)
	, Height(0)
	, Internal_Format(GL_RGBA)
	, Image_Format(GL_RGBA)
	, Wrap_S(GL_REPEAT)
	, Wrap_T(GL_REPEAT)
	, Filter_Min(GL_LINEAR)
	, Filter_Max(GL_LINEAR)
	, ID(0) {}

Texture2D::Texture2D(unsigned int width, unsigned int height)
	: Texture2D() {
	Generate(width, height, (void*)0);
}

Texture2D::Texture2D(unsigned int width, unsigned int height, const void* const data)
	: Texture2D() {
	Generate(width, height, data);
}

Texture2D::Texture2D(Texture2D&& other) noexcept
	: ID(other.ID) {
	other.ID = 0;
}

Texture2D::~Texture2D() noexcept {
	glBindTexture(GL_TEXTURE_2D, 0);
	if (ID > 0)
		glDeleteTextures(1, &ID);
}

void Texture2D::Generate(unsigned int width, unsigned int height, const void* const data) {
	glBindTexture(GL_TEXTURE_2D, 0);
	this->Width = width;
	this->Height = height;
	// create Texture
	glGenTextures(1, &this->ID);
	glBindTexture(GL_TEXTURE_2D, this->ID);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, this->Internal_Format, width, height, 0, this->Image_Format, GL_UNSIGNED_BYTE, data);
	// set Texture wrap and filter modes
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, this->Wrap_S);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, this->Wrap_T);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, this->Filter_Min);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, this->Filter_Max);
	// unbind texture
	glBindTexture(GL_TEXTURE_2D, 0);
}

const Texture2D& Texture2D::Active(unsigned samplerID) const noexcept {
	glActiveTexture(samplerID);
	return *this;
}
const Texture2D& Texture2D::Bind() const {
	glBindTexture(GL_TEXTURE_2D, this->ID);
	return *this;
}

bool Texture2D::Render(const unsigned int fbo) {
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ID, 0);
	static constexpr GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
	glDrawBuffers(1, DrawBuffers);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}

bool Texture2D::Validate() const noexcept { return glIsTexture(ID) == GL_TRUE; }
