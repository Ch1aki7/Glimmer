// Application.cpp
#include "glpch.h"
#include "Application.h"

#include <glad/glad.h>
namespace gl {
    Application::Application() {
        m_Window = std::unique_ptr<Window>(Window::Create());
        // 使用 Lambda 表达式把事件传给 OnEvent
        m_Window->SetEventCallback([this](Event& e) {
            this->OnEvent(e);
            });
        unsigned int id;
        glGenVertexArrays(1, &id);
    }
    Application::~Application() {}

    void Application::PushLayer(Layer* layer) {
        m_LayerStack.PushLayer(layer);
        layer->OnAttach();
    }

    void Application::PushOverlay(Layer* overlay) {
        m_LayerStack.PushOverlay(overlay);
        overlay->OnAttach();
    }

    void Application::OnEvent(Event& e) {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& event) {
            return this->OnWindowClose(event);
            });

        // 核心逻辑：事件倒序分发
        // 为什么要倒序？因为最上层（UI）在 vector 的末尾，它们应该先处理事件
        for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); ) {
            (*--it)->OnEvent(e);
            if (e.Handled) // 如果事件被某一层拦截了，直接停止传递
                break;
        }
    }
    void Application::Run() {

        while (m_Running) {
            // 核心逻辑：正序更新
            // 先渲染背景，再渲染玩家，最后渲染 UI
            for (Layer* layer : m_LayerStack)
                layer->OnUpdate();

            m_Window->OnUpdate();
        }
    }

    bool Application::OnWindowClose(WindowCloseEvent& e) {
        m_Running = false;
        return true;
    }


}