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

        GLenum ToMinFilter(TextureFilter filter)
        {
            switch (filter)
            {
            case TextureFilter::Nearest: return GL_NEAREST;
            case TextureFilter::LinearMipmapLinear:
                return GL_LINEAR_MIPMAP_LINEAR;
            default: return GL_LINEAR;
            }
        }

        GLenum ToMagFilter(TextureFilter filter)
        {
            return filter == TextureFilter::Nearest
                ? GL_NEAREST : GL_LINEAR;
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
        GL_CORE_ASSERT(m_Specification.MipLevels > 0
            && m_Specification.MipLevels
                <= CalculateTextureMipCount(m_Specification.Size),
            "Cubemap mip count is invalid for its size.");

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
            static_cast<GLsizei>(m_Specification.MipLevels),
            internalFormat,
            static_cast<GLsizei>(m_Specification.Size),
            static_cast<GLsizei>(m_Specification.Size));
        glTextureParameteri(
            m_RendererID, GL_TEXTURE_MIN_FILTER,
            ToMinFilter(m_Specification.MinFilter));
        glTextureParameteri(
            m_RendererID, GL_TEXTURE_MAG_FILTER,
            ToMagFilter(m_Specification.MagFilter));
        glTextureParameteri(
            m_RendererID, GL_TEXTURE_BASE_LEVEL, 0);
        glTextureParameteri(
            m_RendererID, GL_TEXTURE_MAX_LEVEL,
            static_cast<GLint>(m_Specification.MipLevels - 1));
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

    uint32_t OpenGLTextureCube::GetFaceTransferSize(uint32_t mipLevel) const
    {
        const uint32_t mipSize = std::max(
            1u, m_Specification.Size >> mipLevel);
        const uint32_t componentSize = m_DataType == GL_FLOAT
            ? sizeof(float) : sizeof(uint8_t);
        return mipSize * mipSize
            * ChannelCount(m_Specification.Format) * componentSize;
    }

    void OpenGLTextureCube::SetFaceData(
        TextureCubeFace face, const void* data, uint32_t size,
        uint32_t mipLevel)
    {
        GL_CORE_ASSERT(data, "Cubemap face data cannot be null.");
        GL_CORE_ASSERT(mipLevel < m_Specification.MipLevels,
            "Cubemap mip level is out of range.");
        GL_CORE_ASSERT(size == GetFaceTransferSize(mipLevel),
            "Cubemap face upload size does not match specification.");
        const uint32_t mipSize = std::max(
            1u, m_Specification.Size >> mipLevel);

        glTextureSubImage3D(
            m_RendererID,
            static_cast<GLint>(mipLevel),
            0,
            0,
            static_cast<GLint>(face),
            static_cast<GLsizei>(mipSize),
            static_cast<GLsizei>(mipSize),
            1,
            m_DataFormat,
            m_DataType,
            data);
    }

    bool OpenGLTextureCube::GetFaceFloatData(
        TextureCubeFace face, float* data, uint32_t componentCount,
        uint32_t mipLevel) const
    {
        if (!data || mipLevel >= m_Specification.MipLevels)
            return false;
        const uint32_t mipSize = std::max(
            1u, m_Specification.Size >> mipLevel);
        const uint32_t expectedComponents = mipSize * mipSize * 4;
        if (componentCount != expectedComponents)
            return false;

        glGetTextureSubImage(
            m_RendererID,
            static_cast<GLint>(mipLevel),
            0,
            0,
            static_cast<GLint>(face),
            static_cast<GLsizei>(mipSize),
            static_cast<GLsizei>(mipSize),
            1,
            GL_RGBA,
            GL_FLOAT,
            static_cast<GLsizei>(componentCount * sizeof(float)),
            data);
        return true;
    }

    void OpenGLTextureCube::GenerateMipmaps()
    {
        if (m_Specification.MipLevels > 1)
            glGenerateTextureMipmap(m_RendererID);
    }

    void OpenGLTextureCube::Bind(uint32_t slot) const
    {
        glBindTextureUnit(slot, m_RendererID);
    }

}
