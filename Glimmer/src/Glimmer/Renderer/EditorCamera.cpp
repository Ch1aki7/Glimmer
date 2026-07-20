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
		// 右键拖拽 → 旋转
		if (Input::IsMouseButtonPressed(GL_MOUSE_BUTTON_RIGHT))
		{
			auto [x, y] = Input::GetMousePosition();
			glm::vec2 mouse = { x, y };
			glm::vec2 delta = (mouse - m_InitialRightMouse) * m_RotationSpeed;
			m_Yaw   += delta.x;
			m_Pitch -= delta.y;
			m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f); // 限制俯仰
			m_InitialRightMouse = mouse;
			UpdateView();
		}
		else
		{
			m_InitialRightMouse = { Input::GetMousePosition().first, Input::GetMousePosition().second };
		}

		// 中键拖拽 → 平移
		if (Input::IsMouseButtonPressed(GL_MOUSE_BUTTON_MIDDLE))
		{
			auto [x, y] = Input::GetMousePosition();
			glm::vec2 mouse = { x, y };
			glm::vec2 delta = (mouse - m_InitialMiddleMouse) * m_PanSpeed * m_Distance;
			m_FocalPoint += -GetRightDirection() * delta.x + GetUpDirection() * delta.y;
			m_InitialMiddleMouse = mouse;
			UpdateView();
		}
		else
		{
			m_InitialMiddleMouse = { Input::GetMousePosition().first, Input::GetMousePosition().second };
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
