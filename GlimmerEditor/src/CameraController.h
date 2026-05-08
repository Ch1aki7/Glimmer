#pragma once
#include <Glimmer.h>

class CameraController : public gl::ScriptableEntity
{
public:
	void OnUpdate(gl::Timestep ts) override
	{
		auto& transform = GetComponent<gl::TransformComponent>();
		float speed = 5.0f;

		if (gl::Input::IsKeyPressed(GL_KEY_A))
			transform.Translation.x -= speed * ts;
		if (gl::Input::IsKeyPressed(GL_KEY_D))
			transform.Translation.x += speed * ts;
		if (gl::Input::IsKeyPressed(GL_KEY_W))
			transform.Translation.y += speed * ts;
		if (gl::Input::IsKeyPressed(GL_KEY_S))
			transform.Translation.y -= speed * ts;
	}
};
