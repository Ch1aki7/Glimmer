#pragma once

#include "Glimmer/Renderer/TextureCube.h"

#include <glad/glad.h>

namespace gl {

    class OpenGLTextureCube : public TextureCube
    {
    public:
        explicit OpenGLTextureCube(
            const TextureCubeSpecification& specification);
        ~OpenGLTextureCube() override;

        const TextureCubeSpecification& GetSpecification() const override
        {
            return m_Specification;
        }

        void SetFaceData(
            TextureCubeFace face, const void* data, uint32_t size,
            uint32_t mipLevel = 0) override;
        bool GetFaceFloatData(
            TextureCubeFace face, float* data, uint32_t componentCount,
            uint32_t mipLevel = 0) const override;
        void GenerateMipmaps() override;
        void Bind(uint32_t slot = 0) const override;
        uint32_t GetRendererID() const override { return m_RendererID; }

    private:
        uint32_t GetFaceTransferSize(uint32_t mipLevel) const;

    private:
        TextureCubeSpecification m_Specification;
        uint32_t m_RendererID = 0;
        GLenum m_DataFormat = GL_NONE;
        GLenum m_DataType = GL_NONE;
    };

}
