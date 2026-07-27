#include "glpch.h"
#include "TextureCube.h"

#include "Glimmer/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLTextureCube.h"

#include "stb_image.h"

#include <vector>

namespace gl {

    Ref<TextureCube> TextureCube::Create(
        const TextureCubeSpecification& specification)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return CreateRef<OpenGLTextureCube>(specification);
        case RendererAPI::API::None:
            GL_CORE_ASSERT(false, "RendererAPI::None does not support cubemaps.");
            return nullptr;
        case RendererAPI::API::Vulkan:
            GL_CORE_ASSERT(false, "Vulkan cubemaps are not implemented.");
            return nullptr;
        }
        return nullptr;
    }

    Ref<TextureCube> TextureCube::Create(
        const TextureCubeFileSpecification& fileSpecification)
    {
        std::array<std::vector<uint8_t>, 6> facePixels;
        uint32_t faceSize = 0;

        // Cubemap faces use their source orientation; ordinary 2D textures
        // restore vertical flipping in their own loader.
        stbi_set_flip_vertically_on_load(0);

        for (size_t faceIndex = 0;
            faceIndex < fileSpecification.FacePaths.size();
            ++faceIndex)
        {
            const std::filesystem::path& path =
                fileSpecification.FacePaths[faceIndex];
            if (path.empty())
                continue;

            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* data = stbi_load(
                path.string().c_str(),
                &width,
                &height,
                &channels,
                STBI_rgb_alpha);
            if (!data)
            {
                GL_CORE_ERROR(
                    "Failed to load cubemap face: {0}", path.string());
                return nullptr;
            }

            if (width <= 0 || width != height
                || (faceSize != 0
                    && faceSize != static_cast<uint32_t>(width)))
            {
                GL_CORE_ERROR(
                    "Cubemap faces must be square and have matching dimensions: {0}",
                    path.string());
                stbi_image_free(data);
                return nullptr;
            }

            faceSize = static_cast<uint32_t>(width);
            const size_t byteCount =
                static_cast<size_t>(width) * height * 4;
            facePixels[faceIndex].assign(data, data + byteCount);
            stbi_image_free(data);
        }

        if (faceSize == 0)
        {
            GL_CORE_ERROR("Cubemap file specification has no readable faces.");
            return nullptr;
        }

        const size_t faceByteCount =
            static_cast<size_t>(faceSize) * faceSize * 4;
        for (std::vector<uint8_t>& pixels : facePixels)
        {
            if (!pixels.empty())
                continue;

            pixels.resize(faceByteCount);
            for (size_t offset = 0; offset < faceByteCount; offset += 4)
            {
                std::copy(
                    fileSpecification.MissingFaceColor.begin(),
                    fileSpecification.MissingFaceColor.end(),
                    pixels.begin() + offset);
            }
        }

        TextureCubeSpecification textureSpecification;
        textureSpecification.Size = faceSize;
        textureSpecification.Format = TextureFormat::RGBA8;
        textureSpecification.ColorSpace = fileSpecification.ColorSpace;

        Ref<TextureCube> texture = Create(textureSpecification);
        if (!texture)
            return nullptr;

        for (size_t faceIndex = 0; faceIndex < facePixels.size(); ++faceIndex)
        {
            texture->SetFaceData(
                static_cast<TextureCubeFace>(faceIndex),
                facePixels[faceIndex].data(),
                static_cast<uint32_t>(facePixels[faceIndex].size()));
        }

        return texture;
    }
}
