#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Glimmer/Events/Event.h"
#include "Glimmer/Events/MouseEvent.h"
#include "Glimmer/Core/Timestep.h"

namespace gl {

	// 编辑器自由相机 —— 非 ECS 实体，独立驱动
	// 控制: 右键拖拽=旋转, 中键拖拽=平移, 滚轮=Dolly缩放
	class EditorCamera {
	public:
		EditorCamera(float fov = 45.0f, float aspectRatio = 1.777f,
		             float nearClip = 0.1f, float farClip = 1000.0f);

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		// 输出给渲染 / Gizmo
		const glm::mat4& GetViewMatrix()       const { return m_ViewMatrix; }
		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		const glm::vec3& GetPosition()         const { return m_Position; }
		float GetDistance()                    const { return m_Distance; }

		void SetInputEnabled(bool enabled) { m_InputEnabled = enabled; }
		void SetViewportSize(float width, float height);

	private:
		void UpdateProjection();
		void UpdateView();

		glm::vec3 GetUpDirection()    const;
		glm::vec3 GetRightDirection() const;
		glm::vec3 GetForwardDirection() const;
		glm::vec3 CalculatePosition() const;

		bool OnMouseScroll(MouseScrolledEvent& e);

		// 投影参数
		float m_FOV = 45.0f;
		float m_AspectRatio = 1.777f;
		float m_NearClip = 0.1f;
		float m_FarClip = 1000.0f;

		// 球形坐标
		glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };
		float m_Distance = 10.0f;
		float m_Pitch = 0.0f, m_Yaw = 0.0f; // 度

		glm::vec3 m_Position = { 0.0f, 0.0f, 10.0f };
		glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
		glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);

		// 操作状态
		glm::vec2 m_InitialRightMouse = { 0.0f, 0.0f };
		glm::vec2 m_InitialMiddleMouse = { 0.0f, 0.0f };
		float m_PanSpeed = 0.01f;
		float m_RotationSpeed = 0.3f;
		float m_ZoomSpeed = 0.1f;
		float m_MoveSpeed = 8.0f;
		bool m_InputEnabled = false;
	};

}
