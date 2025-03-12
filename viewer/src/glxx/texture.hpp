#pragma once

#include "glxx/object.hpp"

#include <glad/glad.h>
#include <span>

namespace gl {

template <typename Specialization, Specialization EnumValue>
class basic_texture : public object<basic_texture<Specialization, EnumValue>> {
public:
	static constexpr Specialization TextureType = EnumValue;

	basic_texture() noexcept
		: object() {
		glGenTextures(1, &this->_handle);
	}

	virtual ~basic_texture() noexcept { glDeleteTextures(1, &this->_handle); }

	basic_texture& Bind() noexcept {
		glBindTexture(TextureType, this->_handle);
		return *this;
	}
	const basic_texture& Bind() const noexcept {
		glBindTexture(TextureType, this->_handle);
		return *this;
	}
	basic_texture& Unbind() noexcept {
		glBindTexture(TextureType, 0);
		return *this;
	}
	const basic_texture& Unbind() const noexcept {
		glBindTexture(TextureType, 0);
		return *this;
	}
};

enum texture3d {
	Texture3D = GL_TEXTURE_3D,
	Texture2DArray = GL_TEXTURE_2D_ARRAY,
};

template <texture3d TexType>
class basic3d_texture : public basic_texture<texture3d, TexType> {
private:
public:
	using basic3d_texture::TextureType;

	basic3d_texture() noexcept
		: basic_texture() {}

	~basic3d_texture() noexcept {}

	basic3d_texture& init(int width, int height, int depth) {
		glTexStorage3D(TextureType, 1, GL_RGBA8, width, height, depth);
		return *this;
	}

	basic3d_texture& subimage(int layer, int width, int height, std::span<const char> data) {
		glTexSubImage3D(TextureType, 0, 0, 0, layer, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
		return *this;
	}

	basic3d_texture& subimage(int xoffset, int yoffset, int layer, int width, int height, std::span<const char> data) {
		glTexSubImage3D(
			TextureType, 0, xoffset, yoffset, layer, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
		return *this;
	}
};

using array2d_texture = basic3d_texture<texture3d::Texture2DArray>;

} // namespace gl