#pragma once

#include <cassert>
#include <glad/glad.h>
#include <span>
#include <type_traits>
#include <vector>

namespace gl {

enum Mode {
	POINTS = GL_POINTS,
	LINES = GL_LINES,
	LINE_LOOP = GL_LINE_LOOP,
	LINE_STRIP = GL_LINE_STRIP,
	TRIANGLES = GL_TRIANGLES,
	TRIANGLE_STRIP = GL_TRIANGLE_STRIP,
	TRIANGLE_FAN = GL_TRIANGLE_FAN,
};

namespace buffer {

// clang-format off
enum Type : GLenum {
	ARRAY_BUFFER              = GL_ARRAY_BUFFER,
	ATOMIC_COUNTER_BUFFER     = GL_ATOMIC_COUNTER_BUFFER,
	COPY_READ_BUFFER          = GL_COPY_READ_BUFFER,
	COPY_WRITE_BUFFER         = GL_COPY_WRITE_BUFFER,
	DRAW_INDIRECT_BUFFER      = GL_DRAW_INDIRECT_BUFFER,
	DISPATCH_INDIRECT_BUFFER  = GL_DISPATCH_INDIRECT_BUFFER,
	ELEMENT_ARRAY_BUFFER      = GL_ELEMENT_ARRAY_BUFFER,
	PIXEL_PACK_BUFFER         = GL_PIXEL_PACK_BUFFER,
	PIXEL_UNPACK_BUFFER       = GL_PIXEL_UNPACK_BUFFER,
	QUERY_BUFFER              = GL_QUERY_BUFFER,
	SHADER_STORAGE_BUFFER     = GL_SHADER_STORAGE_BUFFER,
	TRANSFORM_FEEDBACK_BUFFER = GL_TRANSFORM_FEEDBACK_BUFFER,
	UNIFORM_BUFFER            = GL_UNIFORM_BUFFER
};

enum Usage : GLenum {
	STREAM_DRAW  = GL_STREAM_DRAW,
	STREAM_READ  = GL_STREAM_READ,
	STREAM_COPY  = GL_STREAM_COPY,
	STATIC_DRAW  = GL_STATIC_DRAW,
	STATIC_READ  = GL_STATIC_READ,
	STATIC_COPY  = GL_STATIC_COPY,
	DYNAMIC_DRAW = GL_DYNAMIC_DRAW,
	DYNAMIC_READ = GL_DYNAMIC_READ,
	DYNAMIC_COPY = GL_DYNAMIC_COPY
};

enum Access {
	READ_ONLY  = GL_READ_ONLY,
	WRITE_ONLY = GL_WRITE_ONLY,
	READ_WRITE = GL_READ_WRITE
};
// clang-format on

} // namespace buffer

template <typename T>
concept standard_layout = std::is_standard_layout_v<T>;

template <
	buffer::Type BufType,
	standard_layout V,
	typename Allocator = std::allocator<V>,
	typename Storage = std::vector<V, Allocator>>
class basic_buffer {
public:
	using value_type = V;
	using allocator_type = Allocator;
	using storage_type = Storage;
	using size_type = storage_type::size_type;

	static constexpr buffer::Type BufferType = BufType;

private:
	GLuint _hnd = 0;
	storage_type _storage;

	basic_buffer(GLuint handle)
		: _hnd(handle) {}

public:
	basic_buffer() { glGenBuffers(1, &_hnd); }
	basic_buffer(const basic_buffer&) = delete;
	basic_buffer(basic_buffer&& other) noexcept
		: _hnd(other._hnd) {
		other._hnd = 0;
		std::swap(_storage, other._storage);
	}
	~basic_buffer() noexcept { glDeleteBuffers(1, &_hnd); }

	GLuint release() {
		const GLuint handle = _hnd;
		_hnd = 0;
		return handle;
	}

	storage_type& storage() noexcept { return _storage; }
	const storage_type& storage() const noexcept { return _storage; }

	// OpenGL functions

	basic_buffer& bind() noexcept {
		glBindBuffer(BufferType, _hnd);
		return *this;
	}
	const basic_buffer& bind() const noexcept {
		glBindBuffer(BufferType, _hnd);
		return *this;
	}
	basic_buffer& unbind() noexcept {
		glBindBuffer(BufferType, 0);
		return *this;
	}
	const basic_buffer& unbind() const noexcept {
		glBindBuffer(BufferType, 0);
		return *this;
	}

	value_type* mapRange(GLintptr offset, GLsizeiptr length, GLbitfield access) const noexcept {
		return glMapBufferRange(BufferType, offset, length, access);
	}

	bool unmap() const noexcept { return glUnmapBuffer(BufferType) == GL_TRUE; }

	const basic_buffer& setData(buffer::Usage u) const noexcept {
		if (!_storage.empty()) {
			glBufferData(BufferType, sizeof(value_type) * _storage.size(), _storage.data(), u);
		}
		return *this;
	}

	basic_buffer& setData(buffer::Usage u, std::span<const value_type> data) noexcept {
		_storage.assign(data.begin(), data.end());
		glBufferData(BufferType, sizeof(value_type) * _storage.size(), _storage.data(), u);
		return *this;
	}

	const basic_buffer& setData(buffer::Usage u, std::span<const value_type> data) const noexcept {
		glBufferData(BufferType, sizeof(value_type) * data.size(), data.data(), u);
		return *this;
	}

	const basic_buffer& setSubData(GLintptr offset, std::span<const value_type> data) const noexcept {
		glBufferSubData(BufferType, offset, sizeof(value_type) * data.size(), data.data());
		return *this;
	}

	GLint64 gl_size() const noexcept {
		GLint64 sz;
		glGetBufferParameteri64v(BufferType, GL_BUFFER_SIZE, &sz);
		return sz;
	}

	const basic_buffer& drawArray(GLint first = 0, Mode m = Mode::TRIANGLES) const noexcept {
		const uint64_t count = gl_size() / sizeof(value_type);
		glDrawArrays(m, first, count);
		return *this;
	}

	// Only for Element Array Buffers
	template <
		buffer::Type BufType = BufferType,
		std::enable_if_t<BufType == buffer::Type::ELEMENT_ARRAY_BUFFER, bool> = true>
	const basic_buffer& drawElements(Mode m = Mode::TRIANGLES) const noexcept {
		constexpr GLenum type = []() {
			if constexpr (std::is_same_v<value_type, std::uint8_t>) {
				return GL_UNSIGNED_BYTE;
			} else if constexpr (std::is_same_v<value_type, std::uint16_t>) {
				return GL_UNSIGNED_SHORT;
			} else if constexpr (std::is_same_v<value_type, std::uint32_t>) {
				return GL_UNSIGNED_INT;
			} else {
				static_assert(false, "drawElements can only be used with uchar, ushort, or uint.");
			}
		}();
		const uint64_t count = gl_size() / sizeof(value_type);
		glDrawElements(m, count, type, 0);
		return *this;
	}

	explicit operator GLuint() const noexcept { return _hnd; }
	explicit operator bool() const noexcept { return (_hnd != 0) && (glIsBuffer(_hnd)); }

	basic_buffer& operator=(const basic_buffer& other) = delete;
	basic_buffer& operator=(basic_buffer&& other) noexcept {
		_hnd = other._hnd;
		other._hnd = 0;
		std::swap(_storage, other._storage);
	}
};

template <typename ValueT>
using ArrayBuffer = basic_buffer<gl::buffer::ARRAY_BUFFER, ValueT>;

template <typename ValueT>
using ElementBuffer = basic_buffer<gl::buffer::ELEMENT_ARRAY_BUFFER, ValueT>;
using ucElementBuffer = ElementBuffer<std::uint8_t>;
using usElementBuffer = ElementBuffer<std::uint16_t>;
using uiElementBuffer = ElementBuffer<std::uint32_t>;

} // namespace gl