#pragma once
#include <glm/glm.hpp>
#include "VertexArray.h"

namespace gl {
    class RendererAPI {
    public:
        enum class API { None = 0, OpenGL = 1, Vulkan = 2 };
    public:
        virtual void Init() = 0;
        virtual void SetClearColor(const glm::vec4& color) = 0;
        virtual void Clear() = 0;
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) = 0;

        inline static API GetAPI() { return s_API; }
        inline static void SetAPI(API api) { s_API = api; }
    private:
        static API s_API;
    };
}
