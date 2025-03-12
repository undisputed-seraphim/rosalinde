#pragma once

namespace gl {

template <typename Impl>
class object {
protected:
	unsigned _handle;

	object() noexcept
		: _handle(0) {}

public:
	object(const object&) = delete;
	object(object&& o) noexcept
		: _handle(o._handle) {
		o._handle = 0;
	}
	virtual ~object() noexcept {}

	virtual explicit operator unsigned() const noexcept { return _handle; }
	virtual explicit operator bool() const noexcept { return _handle != 0; }
};

} // namespace gl