// Application.cpp
#include "glpch.h"
#include "Application.h"
// 渲染器核心
#include "Glimmer/Renderer/Renderer.h"
#include "Glimmer/Renderer/RenderCommand.h"

// 输入与键码
#include "Glimmer/Core/Input.h"
#include "Glimmer/Core/KeyCodes.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
namespace gl {
	Application* Application::s_Instance = nullptr;

	Application::Application() {
		GL_PROFILE_FUNCTION();

		GL_CORE_ASSERT(!s_Instance, "Application already exists!"); // 防止实例化多次
		s_Instance = this; // 【新增】：把自己存入单例

		m_Window = std::unique_ptr<Window>(Window::Create());
		// 使用 Lambda 表达式把事件传给 OnEvent
		m_Window->SetEventCallback([this](Event& e) {
			this->OnEvent(e);
			});

		Renderer::Init();

		m_ImGuiLayer = new ImGuiLayer();
		PushOverlay(m_ImGuiLayer); // 【新增】：把 ImGuiLayer 作为覆盖层推入栈顶
	}

	Application::~Application() {
		GL_PROFILE_FUNCTION();

		Renderer::Shutdown();
	}

	void Application::PushLayer(Layer* layer) {
		GL_PROFILE_FUNCTION();

		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* overlay) {
		GL_PROFILE_FUNCTION();

		m_LayerStack.PushOverlay(overlay);
		overlay->OnAttach();
	}

	void Application::OnEvent(Event& e) {
		GL_PROFILE_FUNCTION();

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
		GL_PROFILE_FUNCTION();

		while (m_Running) {
			GL_PROFILE_SCOPE("Run Loop");

			// glfwGetTime 返回的是从启动到现在的总秒数
			float time = (float)glfwGetTime();
			Timestep timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;

			{
				GL_PROFILE_SCOPE("LayerStack OnUpdate");

				for (Layer* layer : m_LayerStack)
					layer->OnUpdate(timestep);
			}

			m_ImGuiLayer->Begin();
			{
				GL_PROFILE_SCOPE("LayerStack OnImGuiRenderer");

				for (Layer* layer : m_LayerStack)
					layer->OnImGuiRender();
			}
			m_ImGuiLayer->End();

			m_Window->OnUpdate();
		}
	}

	float Application::GetTime()
	{
		return (float)glfwGetTime();
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		GL_PROFILE_FUNCTION();

		m_Running = false;
		return true;
	}


}
