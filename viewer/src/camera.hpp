#pragma once

#include <glm/glm.hpp>

class Camera {
	glm::vec3 _pos;
	glm::vec3 _front;
	glm::vec3 _up;
	float _zoom;

public:
	Camera();
	Camera(float zoom);

	glm::mat4 lookAt() const;
	explicit operator glm::mat4() const;

	void set_zoom(float zoom) { _zoom = zoom; }
	float zoom() const { return _zoom; }
	void recenter(const glm::vec2& pos);
};