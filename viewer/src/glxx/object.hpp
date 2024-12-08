#pragma once

#include <glad/glad.h>
#include <span>

namespace gl {

class OpenGLObject {
protected:
	GLuint _handle;

	OpenGLObject() noexcept
		: _handle(0) {}

public:
	OpenGLObject(const OpenGLObject&) = delete;
	OpenGLObject(OpenGLObject&& o) noexcept
		: _handle(o._handle) {
		o._handle = 0;
	}
	virtual ~OpenGLObject() noexcept {}

	virtual explicit operator GLuint() const noexcept { return _handle; }
	virtual explicit operator bool() const noexcept { return _handle != 0; }

	OpenGLObject& operator=(const OpenGLObject& other) = delete;
	OpenGLObject& operator=(OpenGLObject&& other) = default;
};

} // namespace gl