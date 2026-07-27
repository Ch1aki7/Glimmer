#pragma once

#include "Glimmer/Core/Core.h"

#include <glm/glm.hpp>

namespace gl {

    class Shader;
    class TextureCube;

    class SkyboxRenderer
    {
    public:
        static void Init();
        static void Shutdown();

        static void Draw(
            const Ref<TextureCube>& cubemap,
            const Ref<Shader>& shader,
            const glm::mat4& view,
            const glm::mat4& projection,
            float intensity = 1.0f);
    };

}
