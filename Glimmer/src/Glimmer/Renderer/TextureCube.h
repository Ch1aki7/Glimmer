#pragma once

#include "Glimmer/Renderer/Texture.h"

#include <array>
#include <filesystem>

namespace gl {

    enum class TextureCubeFace : uint32_t
    {
        PositiveX = 0,
        NegativeX,
        PositiveY,
        NegativeY,
        PositiveZ,
        NegativeZ
    };

    struct TextureCubeSpecification
    {
        uint32_t Size = 1;
        uint32_t MipLevels = 1;
        TextureFormat Format = TextureFormat::RGBA8;
        TextureFilter MinFilter = TextureFilter::Linear;
        TextureFilter MagFilter = TextureFilter::Linear;
        TextureWrap Wrap = TextureWrap::ClampToEdge;
        TextureColorSpace ColorSpace = TextureColorSpace::SRGB;
    };

    struct TextureCubeFileSpecification
    {
        std::array<std::filesystem::path, 6> FacePaths;
        std::array<uint8_t, 4> MissingFaceColor = { 0, 0, 0, 255 };
        TextureColorSpace ColorSpace = TextureColorSpace::SRGB;
        bool GenerateMipmaps = true;
    };

    struct TextureCubeEquirectangularSpecification
    {
        std::filesystem::path Path;
        uint32_t FaceSize = 0;
        bool GenerateMipmaps = true;
    };

    class TextureCube
    {
    public:
        virtual ~TextureCube() = default;

        virtual const TextureCubeSpecification& GetSpecification() const = 0;
        virtual void SetFaceData(
            TextureCubeFace face, const void* data, uint32_t size,
            uint32_t mipLevel = 0) = 0;
        virtual bool GetFaceFloatData(
            TextureCubeFace face, float* data, uint32_t componentCount,
            uint32_t mipLevel = 0) const = 0;
        virtual void GenerateMipmaps() = 0;
        virtual void Bind(uint32_t slot = 0) const = 0;
        virtual uint32_t GetRendererID() const = 0;

        static Ref<TextureCube> Create(
            const TextureCubeSpecification& specification);
        static Ref<TextureCube> Create(
            const TextureCubeFileSpecification& specification);
        static Ref<TextureCube> Create(
            const TextureCubeEquirectangularSpecification& specification);
    };

    uint32_t CalculateTextureMipCount(uint32_t size);

}
