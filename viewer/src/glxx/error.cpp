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

#include <map>
static const std::map<std::int32_t, std::string> glErrorStr = {
	{GL_INVALID_ENUM, "GL_INVALID_ENUM"},
	{GL_INVALID_VALUE, "GL_INVALID_VALUE"},
	{GL_INVALID_OPERATION, "GL_INVALID_OPERATION"},
	{GL_STACK_OVERFLOW, "GL_STACK_OVERFLOW"},
	{GL_STACK_UNDERFLOW, "GL_STACK_UNDERFLOW"},
	{GL_OUT_OF_MEMORY, "GL_OUT_OF_MEMORY"},
	{GL_INVALID_FRAMEBUFFER_OPERATION, "GL_INVALID_FRAMEBUFFER_OPERATION"},
	{GL_CONTEXT_LOST, "GL_CONTEXT_LOST"},
	{GL_TABLE_TOO_LARGE, "GL_TABLE_TOO_LARGE"},
	{GL_NO_ERROR, "GL_NO_ERROR"},
};

void check_or_throw() {
	const GLenum error = glGetError();
	if (error == GL_NO_ERROR) {
	    std::cout << "No error." << std::endl;
	}
	auto iter = glErrorStr.find(error);
	auto st = std::to_string(std::stacktrace::current());
	if (iter == std::end(glErrorStr)) {
		throw std::runtime_error("Unknown OpenGL error " + std::to_string(error) + '\t' + std::move(st));
	}
	throw std::runtime_error(iter->second + '\t' + std::move(st));
}