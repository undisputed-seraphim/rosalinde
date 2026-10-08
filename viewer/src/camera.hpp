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
	void fit_bounds(const glm::vec4& bounds, float margin = 0.0f);

	void set_target(const glm::vec2& target);
	void update_follow(float dt);

private:
	bool _following = false;
	glm::vec2 _target{0, 0};
	static constexpr float kFollowSpeed = 6.0f;
};