// SandboxApp.cpp
#include <Glimmer.h>
#include "Platform/OpenGL/OpenGLShader.h"
#include <glm/gtc/type_ptr.hpp>
class ExampleLayer : public gl::Layer {
public:
    ExampleLayer() :Layer("Example"), m_Camera(-1.6f, 1.6f, -0.9f, 0.9f)
    {
        // 1. 创建顶点数组
        m_VertexArray.reset(gl::VertexArray::Create());

        // 2. 准备数据
        //float vertices[3 * 3] = {
        //    -1.0f, -0.5f, 0.0f,
        //     1.0f, -0.5f, 0.0f,
        //     0.0f,  0.5f, 0.0f
        //};
        float vertices[4 * 3] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.5f,  0.5f, 0.0f,
            -0.5f,  0.5f, 0.0f
        };

        std::shared_ptr<gl::VertexBuffer> vertexBuffer;
        vertexBuffer.reset(gl::VertexBuffer::Create(vertices, sizeof(vertices)));

        // 3. 设置布局并添加
        vertexBuffer->SetLayout({
            { gl::ShaderDataType::Float3, "a_Position" }
            });
        m_VertexArray->AddVertexBuffer(vertexBuffer);

        // 4. 设置索引
        //uint32_t indices[3] = { 0, 1, 2 };
        uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };
        std::shared_ptr<gl::IndexBuffer> indexBuffer;
        //indexBuffer.reset(gl::IndexBuffer::Create(indices, 3));
        indexBuffer.reset(gl::IndexBuffer::Create(indices, 6));
        m_VertexArray->SetIndexBuffer(indexBuffer);

        // 5. Shader 代码
        std::string vertexSrc = R"(
		#version 330 core
		layout(location = 0) in vec3 a_Position;
        uniform mat4 u_ViewProjection;
        uniform mat4 u_Transform;
		out vec3 v_Position;
        uniform float u_Time;
		void main()
		{
            vec3 pos = a_Position;
            pos.y += sin(pos.x * 5.0 + u_Time) * 0.1;
            v_Position = pos;
            gl_Position = u_ViewProjection * u_Transform * vec4(pos, 1.0);
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

        m_Shader.reset(gl::Shader::Create(vertexSrc, fragmentSrc));
    }

    void OnUpdate(gl::Timestep ts) override {
        // ---------------------------------------------------------
        // A. 摄像机控制
        // ---------------------------------------------------------
        float moveSpeed = 2.0f;
        if (gl::Input::IsKeyPressed(GL_KEY_A))
            m_Camera.SetPosition(m_Camera.GetPosition() + glm::vec3(-moveSpeed * ts, 0, 0));
        else if (gl::Input::IsKeyPressed(GL_KEY_D))
            m_Camera.SetPosition(m_Camera.GetPosition() + glm::vec3(moveSpeed * ts, 0, 0));

        if (gl::Input::IsKeyPressed(GL_KEY_W))
            m_Camera.SetPosition(m_Camera.GetPosition() + glm::vec3(0, moveSpeed * ts, 0));
        else if (gl::Input::IsKeyPressed(GL_KEY_S))
            m_Camera.SetPosition(m_Camera.GetPosition() + glm::vec3(0, -moveSpeed * ts, 0));

        // ---------------------------------------------------------
        // B. 渲染执行 (现在图层负责提交自己的渲染任务)
        // ---------------------------------------------------------
        gl::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
        gl::RenderCommand::Clear();

        gl::Renderer::BeginScene(m_Camera);

        // 准备一个基础的比例矩阵（让方块变小一点）
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

        m_Shader->Bind();
        // 通过 Application 单例获取时间，彻底告别 GLFW
        float time = gl::Application::Get().GetTime();
        m_Shader->UploadUniformFloat("u_Time", time);

        // gl::Renderer::Submit(m_Shader, m_VertexArray);
        // 渲染一个 20x20 的方块阵列
        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 20; x++) {
                // 计算每个方块的位置
                glm::vec3 pos(x * 0.11f, y * 0.11f, 0.0f);
                float rotation = time * 20.0f + (x + y) * 10.0f;

                glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) *
                    glm::rotate(glm::mat4(1.0f), glm::radians(rotation), { 0, 0, 1 }) *
                    scale;

                // 提交给渲染器，每个方块用不同的 transform
                gl::Renderer::Submit(m_Shader, m_VertexArray, transform);
            }
        }

        gl::Renderer::EndScene();
    }

    void OnEvent(gl::Event& event) override {
        GL_TRACE("{0}", event.ToString());
    }

    void OnImGuiRender() override {
        // 画一个经典的测试面板
        ImGui::Begin("Glimmer Test Window");
        ImGui::Text("Hello World! ImGui is Working!");
        ImGui::End();

        // 也可以直接调出 ImGui 自带的超级演示窗口，看看它有多强大：
        bool show_demo_window = true;
        ImGui::ShowDemoWindow(&show_demo_window);
    }
private:
    std::shared_ptr<gl::VertexArray> m_VertexArray;
    std::shared_ptr<gl::Shader> m_Shader;
    gl::OrthographicCamera m_Camera;
};

// 继承 Glimmer 的引擎基类
class Sandbox : public gl::Application {
public:
    Sandbox() 
    {
        PushLayer(new ExampleLayer());
    }
    ~Sandbox() {}
};

// 告诉引擎，我要启动这个沙盒游戏
gl::Application* gl::CreateApplication() 
{
    return new Sandbox();
}
