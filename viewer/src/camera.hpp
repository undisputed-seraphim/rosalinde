#pragma once

#include <glm/glm.hpp>

class Camera {
	glm::vec3 _pos;
	glm::vec3 _front;
	glm::vec3 _up;
	float _yaw, _pitch, _zoom;

public:
	Camera();

	void zoom(float y);

	glm::mat4 lookAt() const;
	explicit operator glm::mat4() const;
};