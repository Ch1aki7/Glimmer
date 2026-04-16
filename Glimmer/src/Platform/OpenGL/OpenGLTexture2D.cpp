#include "glpch.h"
#include "OpenGLTexture2D.h"

#include "stb_image.h"
#include <glad/glad.h>

namespace gl {

	OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
		: m_Path(path)
	{
		GL_PROFILE_FUNCTION();

		int width, height, channels;

		// 关键点 1：垂直翻转图片。
		// 图片原点在左上，OpenGL 原点在左下。如果不翻转，贴图是倒过来的。
		stbi_set_flip_vertically_on_load(1);

		// 从硬盘加载字节数据
		stbi_uc* data = nullptr;
		{
			GL_PROFILE_SCOPE("stbi_load - OpenGLTexture2D::OpenGLTexture2D(const std:string&)");
			data = stbi_load(path.c_str(), &width, &height, &channels, 0);
		}
		GL_CORE_ASSERT(data, "Failed to load image!");

		m_Width = width;
		m_Height = height;

		// 根据图片的通道数自动选择 OpenGL 格式
		GLenum internalFormat = 0, dataFormat = 0;
		if (channels == 4) {
			internalFormat = GL_RGBA8;
			dataFormat = GL_RGBA;
		}
		else if (channels == 3) {
			internalFormat = GL_RGB8;
			dataFormat = GL_RGB;
		}

		m_InternalFormat = internalFormat;
		m_DataFormat = dataFormat;

		GL_CORE_ASSERT(internalFormat & dataFormat, "Format not supported!");

		// 关键点 2：在显存开辟空间
		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);

		// 关键点 3：配置过滤 (Filtering)
		// 放大时使用线性过滤（平滑），缩小时使用邻近过滤（颗粒感，看你需求）
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		// 关键点 4：上传数据
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Width, m_Height, 0, dataFormat, GL_UNSIGNED_BYTE, data);

		// 释放 CPU 端的内存（因为已经传到 GPU 了）
		stbi_image_free(data);
	}

	// 增加一个新的构造函数用于创建空白贴图
	OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height)
		: m_Width(width), m_Height(height)
	{
		GL_PROFILE_FUNCTION();

		m_InternalFormat = GL_RGBA8;
		m_DataFormat = GL_RGBA;

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);

		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	OpenGLTexture2D::~OpenGLTexture2D()
	{
		GL_PROFILE_FUNCTION();

		glDeleteTextures(1, &m_RendererID);
	}

	void OpenGLTexture2D::SetData(void* data, uint32_t size)
	{
		GL_PROFILE_FUNCTION();

		uint32_t bpp = m_DataFormat == GL_RGBA ? 4 : 3;
		GL_CORE_ASSERT(size == m_Width * m_Height * bpp, "Data must be entire texture!");
		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
	}

	void OpenGLTexture2D::Bind(uint32_t slot) const
	{
		GL_PROFILE_FUNCTION();

		// 先激活 Slot（GL_TEXTURE0, GL_TEXTURE1 ...）
		glActiveTexture(GL_TEXTURE0 + slot);
		// 再把 ID 绑定上去
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
	}

}
