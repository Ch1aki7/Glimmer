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
    };

    class TextureCube
    {
    public:
        virtual ~TextureCube() = default;

        virtual const TextureCubeSpecification& GetSpecification() const = 0;
        virtual void SetFaceData(
            TextureCubeFace face, const void* data, uint32_t size) = 0;
        virtual void Bind(uint32_t slot = 0) const = 0;
        virtual uint32_t GetRendererID() const = 0;

        static Ref<TextureCube> Create(
            const TextureCubeSpecification& specification);
        static Ref<TextureCube> Create(
            const TextureCubeFileSpecification& specification);
    };

}
