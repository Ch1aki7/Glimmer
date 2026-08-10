#include "glpch.h"
#include "EditorCamera.h"
#include "Glimmer/Core/Input.h"
#include "Glimmer/Core/KeyCodes.h"
#include "Glimmer/Core/MouseButtonCodes.h"

#include <glm/gtc/matrix_transform.hpp>

namespace gl {

	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
		: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip)
	{
		UpdateProjection();
		UpdateView();
	}

	void EditorCamera::OnUpdate(Timestep ts)
	{
		auto [mouseX, mouseY] = Input::GetMousePosition();
		const glm::vec2 mouse = { mouseX, mouseY };

		if (!m_InputEnabled)
		{
			m_InitialRightMouse = mouse;
			m_InitialMiddleMouse = mouse;
			return;
		}

		if (Input::IsMouseButtonPressed(GL_MOUSE_BUTTON_RIGHT))
		{
			const glm::vec2 delta =
				(mouse - m_InitialRightMouse) * m_RotationSpeed;
			m_Yaw += delta.x;
			m_Pitch = glm::clamp(m_Pitch - delta.y, -89.0f, 89.0f);
			m_InitialRightMouse = mouse;
			UpdateView();

			glm::vec3 moveDirection{ 0.0f };
			if (Input::IsKeyPressed(GL_KEY_W))
				moveDirection += GetForwardDirection();
			if (Input::IsKeyPressed(GL_KEY_S))
				moveDirection -= GetForwardDirection();
			if (Input::IsKeyPressed(GL_KEY_D))
				moveDirection += GetRightDirection();
			if (Input::IsKeyPressed(GL_KEY_A))
				moveDirection -= GetRightDirection();
			if (Input::IsKeyPressed(GL_KEY_E))
				moveDirection += GetUpDirection();
			if (Input::IsKeyPressed(GL_KEY_Q))
				moveDirection -= GetUpDirection();

			if (glm::dot(moveDirection, moveDirection) > 0.0f)
			{
				const bool accelerated =
					Input::IsKeyPressed(GL_KEY_LEFT_SHIFT)
					|| Input::IsKeyPressed(GL_KEY_RIGHT_SHIFT);
				const float speed = m_MoveSpeed * (accelerated ? 3.0f : 1.0f);
				m_FocalPoint += glm::normalize(moveDirection)
					* speed * static_cast<float>(ts);
				UpdateView();
			}
		}
		else
		{
			m_InitialRightMouse = mouse;
		}

		if (Input::IsMouseButtonPressed(GL_MOUSE_BUTTON_MIDDLE))
		{
			const glm::vec2 delta =
				(mouse - m_InitialMiddleMouse) * m_PanSpeed * m_Distance;
			m_FocalPoint += -GetRightDirection() * delta.x
				+ GetUpDirection() * delta.y;
			m_InitialMiddleMouse = mouse;
			UpdateView();
		}
		else
		{
			m_InitialMiddleMouse = mouse;
		}
	}

	void EditorCamera::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(EditorCamera::OnMouseScroll));
	}

	bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e)
	{
		float delta = e.GetYOffset() * m_ZoomSpeed;
		m_Distance -= delta * m_Distance * 0.1f;          // 距离比例缩放
		m_Distance = glm::clamp(m_Distance, 0.5f, 500.0f);
		UpdateView();
		return false;
	}

	void EditorCamera::SetViewportSize(float width, float height)
	{
		m_AspectRatio = width / height;
		UpdateProjection();
	}

	void EditorCamera::SetView(const glm::vec3& focalPoint, float distance,
		float pitchDegrees, float yawDegrees)
	{
		m_FocalPoint = focalPoint;
		m_Distance = glm::clamp(distance, 0.5f, 500.0f);
		m_Pitch = glm::clamp(pitchDegrees, -89.0f, 89.0f);
		m_Yaw = yawDegrees;
		UpdateView();
	}

	void EditorCamera::Focus(const glm::vec3& focalPoint, float distance)
	{
		m_FocalPoint = focalPoint;
		m_Distance = glm::clamp(distance, 0.5f, 500.0f);
		UpdateView();
	}

	void EditorCamera::UpdateProjection()
	{
		m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
	}

	void EditorCamera::UpdateView()
	{
		m_Position = CalculatePosition();
		m_ViewMatrix = glm::lookAt(m_Position, m_FocalPoint, GetUpDirection());
	}

	glm::vec3 EditorCamera::GetUpDirection() const
	{
		return glm::vec3(0.0f, 1.0f, 0.0f);
	}

	glm::vec3 EditorCamera::GetRightDirection() const
	{
		return glm::normalize(glm::cross(GetForwardDirection(), GetUpDirection()));
	}

	glm::vec3 EditorCamera::GetForwardDirection() const
	{
		return glm::normalize(m_FocalPoint - m_Position);
	}

	glm::vec3 EditorCamera::CalculatePosition() const
	{
		glm::quat orientation = glm::angleAxis(glm::radians(-m_Yaw),   glm::vec3(0, 1, 0))
		                      * glm::angleAxis(glm::radians(-m_Pitch), glm::vec3(1, 0, 0));
		return m_FocalPoint - (orientation * glm::vec3(0.0f, 0.0f, 1.0f)) * m_Distance;
	}

}
