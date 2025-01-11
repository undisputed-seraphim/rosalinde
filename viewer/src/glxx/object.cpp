#include "glxx/object.hpp"

namespace gl {

object::object() noexcept
	: _handle(0) {}

object::object(object&& o) noexcept
	: _handle(o._handle) {
	o._handle = 0;
}

object::operator unsigned() const noexcept { return _handle; }
object::operator bool() const noexcept { return _handle != 0; }

} // namespace gl
