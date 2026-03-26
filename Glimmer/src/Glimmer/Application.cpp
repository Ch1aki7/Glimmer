// Application.cpp
#include "glpch.h"
#include "Application.h"

#include "Glimmer/Events/ApplicationEvent.h"
#include "Glimmer/Log.h"

namespace gl {
    Application::Application() {}
    Application::~Application() {}

    void Application::Run() {

        WindowResizeEvent e(1920, 1080);
        GL_TRACE(e);

        while (true) {
            // 这里将是未来游戏的心脏：Game Loop
        }
    }
}