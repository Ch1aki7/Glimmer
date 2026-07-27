#pragma once

namespace gl {

    enum class TextureColorSpace
    {
        Linear = 0,
        SRGB
    };

    enum class TextureSemantic
    {
        Color = 0,
        Normal,
        Data,
        Height
    };

}
