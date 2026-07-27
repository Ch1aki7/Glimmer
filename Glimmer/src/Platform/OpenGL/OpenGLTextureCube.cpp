#include "glpch.h"
#include "OpenGLTextureCube.h"

namespace gl {

    namespace {

        GLenum ToInternalFormat(
            TextureFormat format, TextureColorSpace colorSpace)
        {
            switch (format)
            {
            case TextureFormat::RGB8:
                return colorSpace == TextureColorSpace::SRGB
                    ? GL_SRGB8 : GL_RGB8;
            case TextureFormat::RGBA8:
                return colorSpace == TextureColorSpace::SRGB
                    ? GL_SRGB8_ALPHA8 : GL_RGBA8;
            case TextureFormat::RGBA16F:
                return GL_RGBA16F;
            default:
                return GL_NONE;
            }
        }

        GLenum ToDataFormat(TextureFormat format)
        {
            switch (format)
            {
            case TextureFormat::RGB8: return GL_RGB;
            case TextureFormat::RGBA8:
            case TextureFormat::RGBA16F: return GL_RGBA;
            default: return GL_NONE;
            }
        }

        GLenum ToDataType(TextureFormat format)
        {
            return format == TextureFormat::RGBA16F
                ? GL_FLOAT : GL_UNSIGNED_BYTE;
        }

        uint32_t ChannelCount(TextureFormat format)
        {
            return format == TextureFormat::RGB8 ? 3u : 4u;
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
            case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
            default: return GL_CLAMP_TO_EDGE;
            }
        }

    }

    OpenGLTextureCube::OpenGLTextureCube(
        const TextureCubeSpecification& specification)
        : m_Specification(specification)
    {
        GL_PROFILE_FUNCTION();
        GL_CORE_ASSERT(m_Specification.Size > 0,
            "Cubemap size must be greater than zero.");

        if (m_Specification.ColorSpace == TextureColorSpace::SRGB
            && m_Specification.Format != TextureFormat::RGB8
            && m_Specification.Format != TextureFormat::RGBA8)
        {
            GL_CORE_WARN("sRGB is unsupported for this cubemap format; using Linear.");
            m_Specification.ColorSpace = TextureColorSpace::Linear;
        }

        const GLenum internalFormat = ToInternalFormat(
            m_Specification.Format, m_Specification.ColorSpace);
        m_DataFormat = ToDataFormat(m_Specification.Format);
        m_DataType = ToDataType(m_Specification.Format);
        GL_CORE_ASSERT(internalFormat != GL_NONE && m_DataFormat != GL_NONE,
            "Unsupported cubemap format.");

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
        glTextureStorage2D(
            m_RendererID,
            1,
            internalFormat,
            static_cast<GLsizei>(m_Specification.Size),
            static_cast<GLsizei>(m_Specification.Size));
        glTextureParameteri(
            m_RendererID, GL_TEXTURE_MIN_FILTER, ToFilter(m_Specification.MinFilter));
        glTextureParameteri(
            m_RendererID, GL_TEXTURE_MAG_FILTER, ToFilter(m_Specification.MagFilter));
        glTextureParameteri(
            m_RendererID, GL_TEXTURE_WRAP_S, ToWrap(m_Specification.Wrap));
        glTextureParameteri(
            m_RendererID, GL_TEXTURE_WRAP_T, ToWrap(m_Specification.Wrap));
        glTextureParameteri(
            m_RendererID, GL_TEXTURE_WRAP_R, ToWrap(m_Specification.Wrap));
    }

    OpenGLTextureCube::~OpenGLTextureCube()
    {
        if (m_RendererID != 0)
            glDeleteTextures(1, &m_RendererID);
    }

    uint32_t OpenGLTextureCube::GetFaceTransferSize() const
    {
        const uint32_t componentSize = m_DataType == GL_FLOAT
            ? sizeof(float) : sizeof(uint8_t);
        return m_Specification.Size * m_Specification.Size
            * ChannelCount(m_Specification.Format) * componentSize;
    }

    void OpenGLTextureCube::SetFaceData(
        TextureCubeFace face, const void* data, uint32_t size)
    {
        GL_CORE_ASSERT(data, "Cubemap face data cannot be null.");
        GL_CORE_ASSERT(size == GetFaceTransferSize(),
            "Cubemap face upload size does not match specification.");

        glTextureSubImage3D(
            m_RendererID,
            0,
            0,
            0,
            static_cast<GLint>(face),
            static_cast<GLsizei>(m_Specification.Size),
            static_cast<GLsizei>(m_Specification.Size),
            1,
            m_DataFormat,
            m_DataType,
            data);
    }

    void OpenGLTextureCube::Bind(uint32_t slot) const
    {
        glBindTextureUnit(slot, m_RendererID);
    }

}
