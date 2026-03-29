#pragma once
#include "Glimmer/Renderer/GraphicsContext.h"

struct GLFWwindow; // 前向声明，减少头文件包含

namespace gl {

    class OpenGLContext : public GraphicsContext {
    public:
        OpenGLContext(GLFWwindow* windowHandle);

        virtual void Init() override;
        virtual void SwapBuffers() override;
    private:
        GLFWwindow* m_WindowHandle;
    };

}