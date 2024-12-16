#pragma once

#include <cassert>
#include <glad/glad.h>
#include <span>
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

template <buffer::Type BufType, typename V>
class Buffer {
public:
	using ValueT = V;
	using storage_type = std::vector<ValueT>;
	using size_type = storage_type::size_type;

	static constexpr buffer::Type BufferType = BufType;

private:
	GLuint _hnd = 0;
	std::vector<ValueT> _storage;

	Buffer(GLuint handle)
		: _hnd(handle) {}

public:
	Buffer() { glGenBuffers(1, &_hnd); }
	Buffer(const Buffer&) = delete;
	Buffer(Buffer&& other) noexcept
		: _hnd(other._hnd) {
		other._hnd = 0;
		std::swap(_storage, other._storage);
	}
	~Buffer() noexcept { glDeleteBuffers(1, _hnd); }

	GLuint release() {
		const GLuint handle = _hnd;
		_hnd = 0;
		return handle;
	}

	std::vector<ValueT>& storage() noexcept { return _storage; }
	const std::vector<ValueT>& storage() const noexcept { return _storage; }

	// OpenGL functions

	Buffer& bind() noexcept {
		glBindBuffer(BufferType, _hnd);
		return *this;
	}
	const Buffer& bind() const noexcept {
		glBindBuffer(BufferType, _hnd);
		return *this;
	}
	Buffer& unbind() noexcept {
		glBindBuffer(BufferType, 0);
		return *this;
	}
	const Buffer& unbind() const noexcept {
		glBindBuffer(BufferType, 0);
		return *this;
	}

	ValueT* mapRange(GLintptr offset, GLsizeiptr length, GLbitfield access) const noexcept {
		return glMapBufferRange(BufferType, offset, length, access);
	}

	bool unmap() const noexcept { return glUnmapBuffer(BufferType) == GL_TRUE; }

	const Buffer& setData(buffer::Usage u) const noexcept {
		if (!_storage.empty()) {
			glBufferData(BufferType, sizeof(ValueT) * _storage.size(), _storage.data(), u);
		}
		return *this;
	}

	Buffer& setData(buffer::Usage u, std::span<const ValueT> data) noexcept {
		_storage.assign(data);
		glBufferData(BufferType, sizeof(ValueT) * _storage.size(), _storage.data(), u);
		return *this;
	}

	const Buffer& setData(buffer::Usage u, std::span<const ValueT> data) const noexcept {
		glBufferData(BufferType, sizeof(ValueT) * data.size(), data.data(), u);
		return *this;
	}

	const Buffer& setSubData(GLintptr offset, std::span<const ValueT> data) const noexcept {
		glBufferSubData(BufferType, offset, sizeof(ValueT) * data.size(), data.data());
		return *this;
	}

	GLint64 gl_size() const noexcept {
		GLint64 sz;
		glGetBufferParameteri64v(BufferType, GL_BUFFER_SIZE, &sz);
		return sz;
	}

	const Buffer& drawArray(GLint first = 0, Mode m = Mode::TRIANGLES) const noexcept {
		const uint64_t count = gl_size() / sizeof(ValueT);
		glDrawArrays(m, first, count);
		return *this;
	}

	// Only for Element Array Buffers
	template <
		buffer::Type BufType = BufferType,
		std::enable_if_t<BufType == buffer::Type::ELEMENT_ARRAY_BUFFER, bool> = true>
	const Buffer& drawElements(Mode m = Mode::TRIANGLES) const noexcept {
		constexpr GLenum type = []() {
			if constexpr (std::is_same_v<ValueT, std::uint8_t>) {
				return GL_UNSIGNED_BYTE;
			}
			if constexpr (std::is_same_v<ValueT, std::uint16_t>) {
				return GL_UNSIGNED_SHORT;
			}
			if constexpr (std::is_same_v<ValueT, std::uint32_t>) {
				return GL_UNSIGNED_INT;
			}
			static_assert(false, "drawElements can only be used with uchar, ushort, or uint.");
		}();
		const uint64_t count = gl_size() / sizeof(ValueT);
		glDrawElements(m, count, type);
		return *this;
	}

	explicit operator GLuint() const noexcept { return _hnd; }
	explicit operator bool() const noexcept { return (_hnd != 0) && (glIsBuffer(_hnd)); }

	Buffer& operator=(const Buffer& other) = delete;
	Buffer& operator=(Buffer&& other) noexcept {
		_hnd = other._hnd;
		other._hnd = 0;
		std::swap(_storage, other._storage);
	}
};

template <typename ValueT>
using ArrayBuffer = Buffer<gl::buffer::ARRAY_BUFFER, ValueT>;
using fArrayBuffer = ArrayBuffer<float>;
using iArrayBuffer = ArrayBuffer<int>;

template <typename ValueT>
using ElementBuffer = Buffer<gl::buffer::ELEMENT_ARRAY_BUFFER, ValueT>;
using ucElementBuffer = ElementBuffer<std::uint8_t>;
using usElementBuffer = ElementBuffer<std::uint16_t>;
using uiElementBuffer = ElementBuffer<std::uint32_t>;

} // namespace gl