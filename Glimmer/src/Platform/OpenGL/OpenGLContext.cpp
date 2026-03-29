#include "glpch.h"
#include "OpenGLContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace gl {

    OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
        : m_WindowHandle(windowHandle)
    {
        GL_CORE_ASSERT(windowHandle, "Window handle is null!")
    }

    void OpenGLContext::Init()
    {
        // 1. 将该窗口设为当前的 OpenGL 上下文
        glfwMakeContextCurrent(m_WindowHandle);

        // 2. 使用 Glad 加载 OpenGL 函数指针
        int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        GL_CORE_ASSERT(status, "Failed to initialize Glad!");

        GL_CORE_INFO("OpenGL Info:");
        GL_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
        GL_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
        GL_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));
    }

    void OpenGLContext::SwapBuffers()
    {
        // 交换前后缓冲区
        glfwSwapBuffers(m_WindowHandle);
    }
}