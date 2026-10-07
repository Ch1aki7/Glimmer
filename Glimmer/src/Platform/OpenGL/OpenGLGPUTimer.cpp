#include "glpch.h"
#include "OpenGLGPUTimer.h"

#include <glad/glad.h>

namespace gl {

	OpenGLGPUTimer::OpenGLGPUTimer(bool timestampPairs) : m_TimestampPairs(timestampPairs)
	{
		if (m_TimestampPairs) {
			glCreateQueries(GL_TIMESTAMP, QueryCount, m_QueryIDs.data());
			glCreateQueries(GL_TIMESTAMP, QueryCount, m_EndQueryIDs.data());
		} else glGenQueries(QueryCount, m_QueryIDs.data());
	}

	OpenGLGPUTimer::~OpenGLGPUTimer()
	{
		if (m_Active && !m_TimestampPairs)
			glEndQuery(GL_TIME_ELAPSED);
		if (m_TimestampPairs) glDeleteQueries(QueryCount, m_EndQueryIDs.data());
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
			if (m_TimestampPairs) {
				m_Sequence[index] = m_NextSequence++;
				glQueryCounter(m_QueryIDs[index], GL_TIMESTAMP);
			} else glBeginQuery(GL_TIME_ELAPSED, m_QueryIDs[index]);
			m_Active = true;
			return;
		}
	}

	void OpenGLGPUTimer::End()
	{
		if (!m_Active)
			return;
		if (m_TimestampPairs) glQueryCounter(m_EndQueryIDs[m_ActiveQuery], GL_TIMESTAMP);
		else glEndQuery(GL_TIME_ELAPSED);
		m_Pending[m_ActiveQuery] = true;
		m_Active = false;
	}

	bool OpenGLGPUTimer::TryGetElapsedMilliseconds(float& milliseconds)
	{
		if (m_TimestampPairs) {
			uint32_t oldest = QueryCount;
			for (uint32_t i = 0; i < QueryCount; ++i)
				if (m_Pending[i] && (oldest == QueryCount || m_Sequence[i] < m_Sequence[oldest])) oldest = i;
			if (oldest == QueryCount) return false;
			GLint startReady = GL_FALSE, endReady = GL_FALSE;
			glGetQueryObjectiv(m_QueryIDs[oldest], GL_QUERY_RESULT_AVAILABLE, &startReady);
			glGetQueryObjectiv(m_EndQueryIDs[oldest], GL_QUERY_RESULT_AVAILABLE, &endReady);
			if (!startReady || !endReady) return false;
			GLuint64 start = 0, end = 0;
			glGetQueryObjectui64v(m_QueryIDs[oldest], GL_QUERY_RESULT, &start);
			glGetQueryObjectui64v(m_EndQueryIDs[oldest], GL_QUERY_RESULT, &end);
			m_Pending[oldest] = false;
			milliseconds = end >= start ? float(double(end - start) / 1000000.0) : 0.0f;
			return true;
		}
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
