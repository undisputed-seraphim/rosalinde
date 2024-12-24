#include "camera.hpp"
#include <glm/ext.hpp>

Camera::Camera()
	: _pos(0, 0, 0)
	, _front(0, 0, -1)
	, _up(0, 1, 0)
	, _yaw(-90)
	, _pitch(0)
	, _zoom(1) {}

void Camera::zoom(float y) { _zoom += (y / 10.0); }

glm::mat4 Camera::lookAt() const {
	return glm::scale(glm::lookAt(_pos, _pos + _front, _up), glm::vec3{_zoom, _zoom, 1});
}
Camera::operator glm::mat4() const { return lookAt(); }
