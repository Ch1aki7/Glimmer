#pragma once

#include "Glimmer/Renderer/Camera.h"
#include <glm/glm.hpp>

namespace gl {

	class PerspectiveCamera : public Camera
	{
	public:
		PerspectiveCamera(float fov, float aspectRatio, float nearClip, float farClip);

		void SetProjection(float fov, float aspectRatio, float nearClip, float farClip);

		const glm::vec3& GetPosition() const { return m_Position; }
		void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateViewMatrix(); }

		const glm::vec3& GetTarget() const { return m_Target; }
		void SetTarget(const glm::vec3& target) { m_Target = target; RecalculateViewMatrix(); }

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

	private:
		void RecalculateViewMatrix();

	private:
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ViewProjectionMatrix;

		glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
		glm::vec3 m_Target   = { 0.0f, 0.0f, -1.0f };
	};

}
