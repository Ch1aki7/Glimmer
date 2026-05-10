#pragma once
#include <Glimmer.h>

namespace gl {

class CameraController : public ScriptableEntity
{
public:
	void OnUpdate(Timestep ts) override
	{
		auto& transform = GetComponent<TransformComponent>();
		float speed = 5.0f;

		if (Input::IsKeyPressed(GL_KEY_A))
			transform.Translation.x -= speed * ts;
		if (Input::IsKeyPressed(GL_KEY_D))
			transform.Translation.x += speed * ts;
		if (Input::IsKeyPressed(GL_KEY_W))
			transform.Translation.y += speed * ts;
		if (Input::IsKeyPressed(GL_KEY_S))
			transform.Translation.y -= speed * ts;
	}
};

}
