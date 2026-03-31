#include "glpch.h"
#include "OpenGLTexture2D.h"

#include "stb_image.h"
#include <glad/glad.h>

namespace gl {

	OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
		: m_Path(path)
	{
		int width, height, channels;

		// 关键点 1：垂直翻转图片。
		// 图片原点在左上，OpenGL 原点在左下。如果不翻转，贴图是倒过来的。
		stbi_set_flip_vertically_on_load(1);

		// 从硬盘加载字节数据
		stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
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

	OpenGLTexture2D::~OpenGLTexture2D()
	{
		glDeleteTextures(1, &m_RendererID);
	}

	void OpenGLTexture2D::Bind(uint32_t slot) const
	{
		// 先激活 Slot（GL_TEXTURE0, GL_TEXTURE1 ...）
		glActiveTexture(GL_TEXTURE0 + slot);
		// 再把 ID 绑定上去
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
	}

}