#pragma once

namespace gl {

class object {
protected:
	unsigned _handle;

	object() noexcept;

public:
	object(const object&) = delete;
	object(object&& o) noexcept;
	virtual ~object() noexcept {}

	virtual explicit operator unsigned() const noexcept;
	virtual explicit operator bool() const noexcept;

	object& operator=(const object& other) = delete;
	object& operator=(object&& other) = default;
};

} // namespace gl