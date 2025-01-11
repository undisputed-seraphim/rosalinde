#include "glxx/error.hpp"
#include <iostream>
#include <stacktrace>
#include <stdexcept>
#include <string_view>

namespace {

static std::string_view gl_debug_source_string(GLenum source) {
	switch (source) {
	case GL_DEBUG_SOURCE_API:
		return "GL_DEBUG_SOURCE_API";
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
		return "GL_DEBUG_SOURCE_WINDOW_SYSTEM";
	case GL_DEBUG_SOURCE_SHADER_COMPILER:
		return "GL_DEBUG_SOURCE_SHADER_COMPILER";
	case GL_DEBUG_SOURCE_THIRD_PARTY:
		return "GL_DEBUG_SOURCE_THIRD_PARTY";
	case GL_DEBUG_SOURCE_APPLICATION:
		return "GL_DEBUG_SOURCE_APPLICATION";
	case GL_DEBUG_SOURCE_OTHER:
		return "GL_DEBUG_SOURCE_OTHER";
	}
	std::unreachable();
}

static std::string_view gl_debug_type_string(GLenum type) {
	switch (type) {
	case GL_DEBUG_TYPE_ERROR:
		return "GL_DEBUG_TYPE_ERROR";
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		return "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR";
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
		return "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR";
	case GL_DEBUG_TYPE_PORTABILITY:
		return "GL_DEBUG_TYPE_PORTABILITY";
	case GL_DEBUG_TYPE_PERFORMANCE:
		return "GL_DEBUG_TYPE_PERFORMANCE";
	case GL_DEBUG_TYPE_MARKER:
		return "GL_DEBUG_TYPE_MARKER";
	case GL_DEBUG_TYPE_PUSH_GROUP:
		return "GL_DEBUG_TYPE_PUSH_GROUP";
	case GL_DEBUG_TYPE_POP_GROUP:
		return "GL_DEBUG_TYPE_POP_GROUP";
	case GL_DEBUG_TYPE_OTHER:
		return "GL_DEBUG_TYPE_OTHER";
	}
	std::unreachable();
}

enum Severity {
	ERROR = GL_DEBUG_SEVERITY_HIGH,
	WARN = GL_DEBUG_SEVERITY_MEDIUM,
	INFO = GL_DEBUG_SEVERITY_LOW,
	DEBUG = GL_DEBUG_SEVERITY_NOTIFICATION,
};

static std::string_view gl_debug_severity_str(GLenum severity) {
	switch (severity) {
	case GL_DEBUG_SEVERITY_HIGH:
		return "FATAL";
	case GL_DEBUG_SEVERITY_MEDIUM:
		return "ERROR";
	case GL_DEBUG_SEVERITY_LOW:
		return "WARN ";
	case GL_DEBUG_SEVERITY_NOTIFICATION:
		return "INFO ";
	}
	std::unreachable();
}

} // namespace

void message_callback(
	GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam) {
	std::cout << '[' << gl_debug_severity_str(severity) << "] [" << gl_debug_type_string(type) << "]\t"
			  << std::string_view(message, length) << std::endl;
}

void check_or_throw() {
	std::string_view str;
	const GLenum error = glGetError();
	switch (error) {
	case GL_INVALID_ENUM:
		str = "INVALID_ENUM";
		break;
	case GL_INVALID_VALUE:
		str = "INVALID_VALUE";
		break;
	case GL_INVALID_OPERATION:
		str = "INVALID_OPERATION";
		break;
	case GL_STACK_OVERFLOW:
		str = "STACK_OVERFLOW";
		break;
	case GL_STACK_UNDERFLOW:
		str = "STACK_UNDERFLOW";
		break;
	case GL_OUT_OF_MEMORY:
		str = "OUT_OF_MEMORY";
		break;
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		str = "INVALID_FRAMEBUFFER_OPERATION";
		break;
	case GL_CONTEXT_LOST:
		str = "CONTEXT_LOST";
		break;
	case GL_TABLE_TOO_LARGE:
		str = "TABLE_TOO_LARGE";
		break;
	case GL_NO_ERROR:
	default:
		str = "NO_ERROR";
		break;
	}
	if (error != GL_NO_ERROR) {
		throw std::runtime_error(std::string(str) + '\t' + std::to_string(std::stacktrace::current()));
	}
    std::cout << "No error." << std::endl;
}