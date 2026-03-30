// Application.cpp
#include "glpch.h"
#include "Application.h"
// 渲染器核心
#include "Glimmer/Renderer/Renderer.h"
#include "Glimmer/Renderer/RenderCommand.h"

// 输入与键码
#include "Glimmer/Input.h"
#include "Glimmer/KeyCodes.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
namespace gl {
    Application* Application::s_Instance = nullptr;

    Application::Application() {
        GL_CORE_ASSERT(!s_Instance, "Application already exists!"); // 防止实例化多次
        s_Instance = this; // 【新增】：把自己存入单例
        m_Window = std::unique_ptr<Window>(Window::Create());
        // 使用 Lambda 表达式把事件传给 OnEvent
        m_Window->SetEventCallback([this](Event& e) {
            this->OnEvent(e);
            });

        m_ImGuiLayer = new ImGuiLayer();
        PushOverlay(m_ImGuiLayer); // 【新增】：把 ImGuiLayer 作为覆盖层推入栈顶

        m_Camera.reset(new OrthographicCamera(-1.6f, 1.6f, -0.9f, 0.9f));
        m_VertexArray.reset(VertexArray::Create());

        float vertices[3 * 3] = {
            -1.0f, -0.5f, 0.0f,
             1.0f, -0.5f, 0.0f,
             0.0f,  0.5f, 0.0f
        };

        std::shared_ptr<VertexBuffer> vertexBuffer;
        vertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));

        vertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" }
            });
        m_VertexArray->AddVertexBuffer(vertexBuffer);

        uint32_t indices[3] = { 0, 1, 2 };
        std::shared_ptr<IndexBuffer> indexBuffer;
        indexBuffer.reset(IndexBuffer::Create(indices, 3));
        m_VertexArray->SetIndexBuffer(indexBuffer);

        std::string vertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;

            uniform mat4 u_ViewProjection;

			out vec3 v_Position;

            uniform float u_Time;

			void main()
			{
                vec3 pos = a_Position;
                pos.y += sin(pos.x * 5.0 + u_Time) * 0.1;
                v_Position = pos;
                gl_Position = u_ViewProjection * vec4(pos, 1.0); 
			}
		)";

        std::string fragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 color;

			in vec3 v_Position;
            uniform float u_Time;
			void main()
			{
                vec3 col;
                // 使用三角函数让 R, G, B 三个通道随位置和时间发生不同的相位偏移
                col.r = sin(v_Position.x * 3.0 + u_Time) * 0.5 + 0.5;
                col.g = sin(v_Position.y * 3.0 + u_Time + 2.0) * 0.5 + 0.5;
                col.b = sin((v_Position.x + v_Position.y) * 3.0 + u_Time + 4.0) * 0.5 + 0.5;
    
                color = vec4(col, 1.0);
			}
		)";

        m_Shader.reset(new Shader(vertexSrc, fragmentSrc));
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
            // ---------------------------------------------------------
            // A. 摄像机控制逻辑 (Input Polling)
            // ---------------------------------------------------------
            float moveSpeed = 0.01f;
            if (Input::IsKeyPressed(GL_KEY_A))
                m_Camera->SetPosition({ m_Camera->GetPosition().x - moveSpeed, m_Camera->GetPosition().y, 0.0f });
            else if (Input::IsKeyPressed(GL_KEY_D))
                m_Camera->SetPosition({ m_Camera->GetPosition().x + moveSpeed, m_Camera->GetPosition().y, 0.0f });

            if (Input::IsKeyPressed(GL_KEY_W))
                m_Camera->SetPosition({ m_Camera->GetPosition().x, m_Camera->GetPosition().y + moveSpeed, 0.0f });
            else if (Input::IsKeyPressed(GL_KEY_S))
                m_Camera->SetPosition({ m_Camera->GetPosition().x, m_Camera->GetPosition().y - moveSpeed, 0.0f });

            // ---------------------------------------------------------
            // B. 渲染执行
            // ---------------------------------------------------------
            RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
            RenderCommand::Clear();

            // 1. 开启场景渲染 (传入当前摄像机)
            Renderer::BeginScene(*m_Camera);

            // 2. 更新 Shader 中的时间 Uniform
            m_Shader->Bind();
            m_Shader->UploadUniformFloat("u_Time", (float)glfwGetTime());

            // 3. 提交物体进行渲染
            Renderer::Submit(m_Shader, m_VertexArray);

            // 4. 结束渲染
            Renderer::EndScene();

            // ---------------------------------------------------------
            // C. 层级更新与 UI 渲染
            // ---------------------------------------------------------
            for (Layer* layer : m_LayerStack)
                layer->OnUpdate();

            m_ImGuiLayer->Begin();
            for (Layer* layer : m_LayerStack)
                layer->OnImGuiRender();
            m_ImGuiLayer->End();

            m_Window->OnUpdate();
        }
    }

    bool Application::OnWindowClose(WindowCloseEvent& e) 
    {
        m_Running = false;
        return true;
    }


}