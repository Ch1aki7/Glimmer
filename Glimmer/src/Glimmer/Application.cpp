// Application.cpp
#include "glpch.h"
#include "Application.h"

namespace gl {
    Application::Application() {
        m_Window = std::unique_ptr<Window>(Window::Create());
        // 使用 Lambda 表达式把事件传给 OnEvent
        m_Window->SetEventCallback([this](Event& e) {
            this->OnEvent(e);
            });
    }
    Application::~Application() {}

    void Application::OnEvent(Event& e) {
        GL_CORE_TRACE("{0}", e);

        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& event) {
            return this->OnWindowClose(event);
            });
    }
    void Application::Run() {

        while (m_Running) {
            m_Window->OnUpdate();
        }
    }

    bool Application::OnWindowClose(WindowCloseEvent& e) {
        m_Running = false;
        return true;
    }


}