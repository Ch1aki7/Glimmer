#include "glpch.h"
#include "OpenGLTexture2D.h"

#include "stb_image.h"

#include <algorithm>
#include <array>

namespace gl {

	namespace {

		GLenum ToInternalFormat(TextureFormat format)
		{
			switch (format)
			{
			case TextureFormat::R8: return GL_R8;
			case TextureFormat::RGB8: return GL_RGB8;
			case TextureFormat::RGBA8: return GL_RGBA8;
			case TextureFormat::R16F: return GL_R16F;
			case TextureFormat::RG16F: return GL_RG16F;
			case TextureFormat::RGBA16F: return GL_RGBA16F;
			case TextureFormat::R32F: return GL_R32F;
			default: return GL_NONE;
			}
		}

		GLenum ToDataFormat(TextureFormat format)
		{
			switch (format)
			{
			case TextureFormat::R8:
			case TextureFormat::R16F:
			case TextureFormat::R32F: return GL_RED;
			case TextureFormat::RG16F: return GL_RG;
			case TextureFormat::RGB8: return GL_RGB;
			case TextureFormat::RGBA8:
			case TextureFormat::RGBA16F: return GL_RGBA;
			default: return GL_NONE;
			}
		}

		GLenum ToDataType(TextureFormat format)
		{
			switch (format)
			{
			case TextureFormat::R8:
			case TextureFormat::RGB8:
			case TextureFormat::RGBA8: return GL_UNSIGNED_BYTE;
			case TextureFormat::R16F:
			case TextureFormat::RG16F:
			case TextureFormat::RGBA16F:
			case TextureFormat::R32F: return GL_FLOAT;
			default: return GL_NONE;
			}
		}

		uint32_t ChannelCount(TextureFormat format)
		{
			switch (format)
			{
			case TextureFormat::R8:
			case TextureFormat::R16F:
			case TextureFormat::R32F: return 1;
			case TextureFormat::RG16F: return 2;
			case TextureFormat::RGB8: return 3;
			case TextureFormat::RGBA8:
			case TextureFormat::RGBA16F: return 4;
			default: return 0;
			}
		}

		GLenum ToFilter(TextureFilter filter)
		{
			return filter == TextureFilter::Linear ? GL_LINEAR : GL_NEAREST;
		}

		GLenum ToWrap(TextureWrap wrap)
		{
			switch (wrap)
			{
			case TextureWrap::Repeat: return GL_REPEAT;
			case TextureWrap::ClampToEdge: return GL_CLAMP_TO_EDGE;
			case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
			}
			return GL_REPEAT;
		}

	}

	OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
		: m_Path(path)
	{
		GL_PROFILE_FUNCTION();

		stbi_set_flip_vertically_on_load(1);
		int width = 0;
		int height = 0;
		int channels = 0;
		stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
		GL_CORE_ASSERT(data, "Failed to load texture: {0}", path);

		m_Specification.Width = static_cast<uint32_t>(width);
		m_Specification.Height = static_cast<uint32_t>(height);
		m_Specification.Format = channels == 4
			? TextureFormat::RGBA8
			: channels == 3 ? TextureFormat::RGB8
			: channels == 1 ? TextureFormat::R8
			: TextureFormat::None;
		GL_CORE_ASSERT(m_Specification.Format != TextureFormat::None,
			"Unsupported texture channel count: {0}", channels);

		CreateStorage();
		SetData(data, GetTransferSize());
		stbi_image_free(data);
	}

	OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& specification)
		: m_Specification(specification)
	{
		GL_PROFILE_FUNCTION();
		GL_CORE_ASSERT(m_Specification.Width > 0 && m_Specification.Height > 0,
			"Texture dimensions must be greater than zero.");
		GL_CORE_ASSERT(m_Specification.Format != TextureFormat::None,
			"Texture format must be specified.");
		CreateStorage();
	}

	OpenGLTexture2D::~OpenGLTexture2D()
	{
		if (m_RendererID != 0)
			glDeleteTextures(1, &m_RendererID);
	}

	void OpenGLTexture2D::CreateStorage()
	{
		m_InternalFormat = ToInternalFormat(m_Specification.Format);
		m_DataFormat = ToDataFormat(m_Specification.Format);
		m_DataType = ToDataType(m_Specification.Format);
		GL_CORE_ASSERT(m_InternalFormat != GL_NONE && m_DataFormat != GL_NONE && m_DataType != GL_NONE,
			"Unsupported texture format.");

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(
			m_RendererID,
			1,
			m_InternalFormat,
			static_cast<GLsizei>(m_Specification.Width),
			static_cast<GLsizei>(m_Specification.Height));
		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, ToFilter(m_Specification.MinFilter));
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, ToFilter(m_Specification.MagFilter));
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, ToWrap(m_Specification.WrapS));
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, ToWrap(m_Specification.WrapT));
	}

	uint32_t OpenGLTexture2D::GetTransferSize() const
	{
		const uint32_t componentSize = m_DataType == GL_FLOAT ? sizeof(float) : sizeof(uint8_t);
		return m_Specification.Width * m_Specification.Height
			* ChannelCount(m_Specification.Format) * componentSize;
	}

	void OpenGLTexture2D::SetData(const void* data, uint32_t size)
	{
		GL_CORE_ASSERT(data, "Texture data cannot be null.");
		GL_CORE_ASSERT(size == GetTransferSize(), "Texture upload size does not match specification.");
		glTextureSubImage2D(
			m_RendererID,
			0,
			0,
			0,
			static_cast<GLsizei>(m_Specification.Width),
			static_cast<GLsizei>(m_Specification.Height),
			m_DataFormat,
			m_DataType,
			data);
	}

	void OpenGLTexture2D::GetImageData(void* buffer, uint32_t size) const
	{
		GL_CORE_ASSERT(buffer, "Texture readback buffer cannot be null.");
		GL_CORE_ASSERT(size >= GetTransferSize(), "Texture readback buffer is too small.");
		glGetTextureImage(m_RendererID, 0, m_DataFormat, m_DataType, size, buffer);
	}

	void OpenGLTexture2D::Clear(const glm::vec4& value)
	{
		if (m_DataType == GL_FLOAT)
		{
			const std::array<float, 4> clearValue = { value.r, value.g, value.b, value.a };
			glClearTexImage(m_RendererID, 0, m_DataFormat, GL_FLOAT, clearValue.data());
		}
		else
		{
			const std::array<uint8_t, 4> clearValue = {
				static_cast<uint8_t>(std::clamp(value.r, 0.0f, 1.0f) * 255.0f),
				static_cast<uint8_t>(std::clamp(value.g, 0.0f, 1.0f) * 255.0f),
				static_cast<uint8_t>(std::clamp(value.b, 0.0f, 1.0f) * 255.0f),
				static_cast<uint8_t>(std::clamp(value.a, 0.0f, 1.0f) * 255.0f)
			};
			glClearTexImage(m_RendererID, 0, m_DataFormat, GL_UNSIGNED_BYTE, clearValue.data());
		}
	}

	void OpenGLTexture2D::Bind(uint32_t slot) const
	{
		glBindTextureUnit(slot, m_RendererID);
	}

}