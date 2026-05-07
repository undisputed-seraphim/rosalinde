#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

class Camera {
	glm::vec3 _pos;
	glm::vec3 _front;
	glm::vec3 _up;
	float _zoom;
	bool _move;

	void move(float xrel, float yrel);
	void zoom(float y);

public:
	Camera();
	Camera(float zoom);

	void handleInput(const SDL_Event&);

	glm::mat4 lookAt() const;
	explicit operator glm::mat4() const;
	void fit_bounds(const glm::vec4& bounds);
};