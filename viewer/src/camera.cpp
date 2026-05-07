#include "camera.hpp"
#include <algorithm>
#include <glm/ext.hpp>

Camera::Camera()
	: _pos(0, 0, 0)
	, _front(0, 0, -1)
	, _up(0, 1, 0)
	, _zoom(1)
	, _move(false) {}

Camera::Camera(float zoom)
	: Camera() {
	_zoom = zoom;
}

void Camera::handleInput(const SDL_Event& event) {
	if (event.type == SDL_EVENT_MOUSE_WHEEL) {
		zoom(event.wheel.y);
	}
	if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		_move = true;
	}
	if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
		_move = false;
	}
	if (event.type == SDL_EVENT_MOUSE_MOTION) {
		move(event.motion.xrel, event.motion.yrel);
	}
}

void Camera::move(float x, float y) {
	if (!_move)
		return;
	_pos -= glm::vec3{x, y, 0.0};
}
void Camera::zoom(float y) { _zoom += (y / 10.0); }

glm::mat4 Camera::lookAt() const {
	return glm::scale(glm::lookAt(_pos, _pos + _front, _up), glm::vec3{_zoom, _zoom, 1});
}
Camera::operator glm::mat4() const { return lookAt(); }

void Camera::fit_bounds(const glm::vec4& bounds) {
	static constexpr int W = 1920, H = 1080;
	float cx = (bounds.x + bounds.z) / 2.0f;
	float cy = (bounds.y + bounds.w) / 2.0f;
	float bw = bounds.z - bounds.x;
	float bh = bounds.w - bounds.y;
	if (bw <= 0.0f || bh <= 0.0f) return;
	_zoom = std::min((float)W / bw, (float)H / bh);
	_pos = {cx, cy, 0.0f};
}
