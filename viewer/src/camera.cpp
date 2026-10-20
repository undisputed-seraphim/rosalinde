#include "camera.hpp"

#include <glm/ext.hpp>

Camera::Camera()
	: _pos(0, 0, 0)
	, _front(0, 0, -1)
	, _up(0, 1, 0)
	, _zoom(1) {}

Camera::Camera(float zoom)
	: Camera() {
	_zoom = zoom;
}

glm::mat4 Camera::lookAt() const {
	return glm::scale(glm::lookAt(_pos, _pos + _front, _up), glm::vec3{_zoom, _zoom, 1});
}

Camera::operator glm::mat4() const { return lookAt(); }

void Camera::recenter(const glm::vec2& pos) {
	_pos = {pos.x, pos.y, 0.0f};
}
