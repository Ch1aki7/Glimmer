#include "glpch.h"
#include "PerspectiveCamera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace gl {

	PerspectiveCamera::PerspectiveCamera(float fov, float aspectRatio, float nearClip, float farClip)
		: Camera(glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip)), m_ViewMatrix(1.0f)
	{
		m_ViewProjectionMatrix = m_Projection * m_ViewMatrix;
	}

	void PerspectiveCamera::SetProjection(float fov, float aspectRatio, float nearClip, float farClip)
	{
		m_Projection = glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip);
		m_ViewProjectionMatrix = m_Projection * m_ViewMatrix;
	}

	void PerspectiveCamera::RecalculateViewMatrix()
	{
		m_ViewMatrix = glm::lookAt(m_Position, m_Target, { 0.0f, 1.0f, 0.0f });
		m_ViewProjectionMatrix = m_Projection * m_ViewMatrix;
	}

}
