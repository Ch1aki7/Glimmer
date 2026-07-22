#include "glpch.h"
#include "OpenGLPixelBuffer.h"
#include <glad/glad.h>

namespace gl {

	OpenGLPixelBuffer::OpenGLPixelBuffer(uint32_t width, uint32_t height, uint32_t channels)
		: m_Width(width), m_Height(height), m_Channels(channels)
	{
		m_Size = width * height * channels;

		GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
		glCreateBuffers(2, m_PBO);
		for (int i = 0; i < 2; i++)
		{
			glNamedBufferData(m_PBO[i], m_Size, nullptr, GL_STREAM_READ);
		}
	}

	OpenGLPixelBuffer::~OpenGLPixelBuffer()
	{
		glDeleteBuffers(2, m_PBO);
	}

	void OpenGLPixelBuffer::BeginRead(uint32_t textureID)
	{
		// 绑定纹理的 FBO 上下文不在本类管理，需外部保证绑定
		// 此处假设调用者已绑定 Framebuffer 或纹理已绑定到当前 FBO
		// 实际使用中通过 glGetTextureImage 更简单（不依赖 FBO 绑定）

		// 切换到当前写入 PBO
		glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO[m_ReadIndex]);

		GLenum format = (m_Channels == 4) ? GL_RGBA : GL_RGB;
		glGetTextureImage(textureID, 0, format, GL_UNSIGNED_BYTE, m_Size, nullptr);
		// 数据异步传输到 PBO，不阻塞

		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		// 上一帧的 Ready PBO 变成可 map
		m_MapIndex = m_ReadIndex;
		m_ReadIndex = (m_ReadIndex + 1) % 2;  // 交换
		m_Ready = true;
	}

	const void* OpenGLPixelBuffer::Map()
	{
		if (!m_Ready || m_MapIndex < 0) return nullptr;

		glBindBuffer(GL_PIXEL_PACK_BUFFER, m_PBO[m_MapIndex]);
		return glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
	}

	void OpenGLPixelBuffer::Unmap()
	{
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	}

}
