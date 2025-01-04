#pragma once

#include <glm/glm.hpp>

class Camera {
	glm::vec3 _pos;
	glm::vec3 _front;
	glm::vec3 _up;
	float _zoom;
	bool _move;

public:
	Camera();

	void move(float xrel, float yrel);
	void zoom(float y);

	void enter() noexcept;
	void exit() noexcept;

	glm::mat4 lookAt() const;
	explicit operator glm::mat4() const;
};