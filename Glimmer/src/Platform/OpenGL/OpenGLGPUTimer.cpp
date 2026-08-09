#include "glpch.h"
#include "OpenGLGPUTimer.h"

#include <glad/glad.h>

namespace gl {

	OpenGLGPUTimer::OpenGLGPUTimer()
	{
		glGenQueries(static_cast<GLsizei>(m_QueryIDs.size()), m_QueryIDs.data());
	}

	OpenGLGPUTimer::~OpenGLGPUTimer()
	{
		if (m_Active)
			glEndQuery(GL_TIME_ELAPSED);
		glDeleteQueries(static_cast<GLsizei>(m_QueryIDs.size()), m_QueryIDs.data());
	}

	void OpenGLGPUTimer::Begin()
	{
		if (m_Active)
			return;
		for (uint32_t offset = 0; offset < QueryCount; ++offset)
		{
			const uint32_t index = (m_NextQuery + offset) % QueryCount;
			if (m_Pending[index])
				continue;
			m_ActiveQuery = index;
			m_NextQuery = (index + 1) % QueryCount;
			glBeginQuery(GL_TIME_ELAPSED, m_QueryIDs[index]);
			m_Active = true;
			return;
		}
	}

	void OpenGLGPUTimer::End()
	{
		if (!m_Active)
			return;
		glEndQuery(GL_TIME_ELAPSED);
		m_Pending[m_ActiveQuery] = true;
		m_Active = false;
	}

	bool OpenGLGPUTimer::TryGetElapsedMilliseconds(float& milliseconds)
	{
		bool resultAvailable = false;
		for (uint32_t index = 0; index < QueryCount; ++index)
		{
			if (!m_Pending[index])
				continue;
			GLint available = GL_FALSE;
			glGetQueryObjectiv(m_QueryIDs[index], GL_QUERY_RESULT_AVAILABLE, &available);
			if (available == GL_FALSE)
				continue;
			GLuint64 nanoseconds = 0;
			glGetQueryObjectui64v(m_QueryIDs[index], GL_QUERY_RESULT, &nanoseconds);
			m_Pending[index] = false;
			milliseconds = static_cast<float>(nanoseconds) / 1000000.0f;
			resultAvailable = true;
		}
		return resultAvailable;
	}

}
