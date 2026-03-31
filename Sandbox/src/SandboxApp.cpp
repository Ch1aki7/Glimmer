// SandboxApp.cpp
#include <Glimmer.h>
#include "Platform/OpenGL/OpenGLShader.h"
#include <glm/gtc/type_ptr.hpp>
class ExampleLayer : public gl::Layer {
public:
    ExampleLayer() :Layer("Example"), m_Camera(-1.6f, 1.6f, -0.9f, 0.9f)
    {
        m_VertexArray.reset(gl::VertexArray::Create());

        //float vertices[3 * 3] = {
        //    -1.0f, -0.5f, 0.0f,
        //     1.0f, -0.5f, 0.0f,
        //     0.0f,  0.5f, 0.0f
        //};
        float vertices[4 * 5] = {
            // X, Y, Z          // U, V (0-1范围)
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, // 左下
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f, // 右下
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f, // 右上
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f  // 左上
        };

        std::shared_ptr<gl::VertexBuffer> vertexBuffer;
        vertexBuffer.reset(gl::VertexBuffer::Create(vertices, sizeof(vertices)));

        vertexBuffer->SetLayout({
            { gl::ShaderDataType::Float3, "a_Position" },
            { gl::ShaderDataType::Float2, "a_TexCoord" }
            });
        m_VertexArray->AddVertexBuffer(vertexBuffer);

        //uint32_t indices[3] = { 0, 1, 2 };
        uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };
        std::shared_ptr<gl::IndexBuffer> indexBuffer;
        //indexBuffer.reset(gl::IndexBuffer::Create(indices, 3));
        indexBuffer.reset(gl::IndexBuffer::Create(indices, 6));
        m_VertexArray->SetIndexBuffer(indexBuffer);

        std::string vertexSrc = R"(
		#version 330 core
		layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec2 a_TexCoord;
        uniform mat4 u_ViewProjection;
        uniform mat4 u_Transform;
        uniform float u_Time;
		out vec3 v_Position;
        out vec2 v_TexCoord;
		void main()
		{
            v_TexCoord = a_TexCoord;
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
		in vec2 v_TexCoord;
		uniform sampler2D u_Texture;
        uniform float u_Time;
		void main()
		{
            vec3 col;
            // 使用三角函数让 R, G, B 三个通道随位置和时间发生不同的相位偏移
            col.r = sin(v_Position.x * 3.0 + u_Time) * 0.5 + 0.5;
            col.g = sin(v_Position.y * 3.0 + u_Time + 2.0) * 0.5 + 0.5;
            col.b = sin((v_Position.x + v_Position.y) * 3.0 + u_Time + 4.0) * 0.5 + 0.5;
            color = vec4(col, 1.0);
            color *= texture(u_Texture, v_TexCoord);
		}
	)";

        // m_Shader.reset(gl::Shader::Create(vertexSrc, fragmentSrc));
        m_TextureShader.reset(gl::Shader::Create(vertexSrc, fragmentSrc));
        m_Texture = gl::Texture2D::Create("assets/textures/Henry.jpg");

        std::dynamic_pointer_cast<gl::OpenGLShader>(m_TextureShader)->Bind();
        std::dynamic_pointer_cast<gl::OpenGLShader>(m_TextureShader)->UploadUniformInt("u_Texture", 0);
    }

    void OnUpdate(gl::Timestep ts) override {
        // ---------------------------------------------------------
        // A. 摄像机控制
        // ---------------------------------------------------------
        float moveSpeed = 2.0f;
        float rotationSpeed = 90.0f;
        if (gl::Input::IsKeyPressed(GL_KEY_A))
            m_Camera.SetPosition(m_Camera.GetPosition() + glm::vec3(-moveSpeed * ts, 0, 0));
        else if (gl::Input::IsKeyPressed(GL_KEY_D))
            m_Camera.SetPosition(m_Camera.GetPosition() + glm::vec3(moveSpeed * ts, 0, 0));

        if (gl::Input::IsKeyPressed(GL_KEY_W))
            m_Camera.SetPosition(m_Camera.GetPosition() + glm::vec3(0, moveSpeed * ts, 0));
        else if (gl::Input::IsKeyPressed(GL_KEY_S))
            m_Camera.SetPosition(m_Camera.GetPosition() + glm::vec3(0, -moveSpeed * ts, 0));

        if (gl::Input::IsKeyPressed(GL_KEY_Q))
            m_Camera.SetRotation(m_Camera.GetRotation() + rotationSpeed * ts);
        else if (gl::Input::IsKeyPressed(GL_KEY_E))
            m_Camera.SetRotation(m_Camera.GetRotation() - rotationSpeed * ts);

        // ---------------------------------------------------------
        // B. 渲染执行 (现在图层负责提交自己的渲染任务)
        // ---------------------------------------------------------
        gl::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
        gl::RenderCommand::Clear();

        gl::Renderer::BeginScene(m_Camera);

        // 准备一个基础的比例矩阵（让方块变小一点）
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

        // m_Shader->Bind();

        // 通过 Application 单例获取时间，彻底告别 GLFW
        float time = gl::Application::Get().GetTime();
        m_TextureShader->UploadUniformFloat("u_Time", time);

        //m_Shader->UploadUniformFloat("u_Time", time);
        
        // gl::Renderer::Submit(m_Shader, m_VertexArray);
    
        //// 渲染一个 20x20 的方块阵列
        //for (int y = 0; y < 20; y++) {
        //    for (int x = 0; x < 20; x++) {
        //        // 计算每个方块的位置
        //        glm::vec3 pos(x * 0.11f, y * 0.11f, 0.0f);
        //        float rotation = time * 20.0f + (x + y) * 10.0f;

        //        glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) *
        //            glm::rotate(glm::mat4(1.0f), glm::radians(rotation), { 0, 0, 1 }) *
        //            scale;

        //        gl::Renderer::Submit(m_Shader, m_VertexArray, transform);
        //    }
        //}

        m_Texture->Bind();
        gl::Renderer::Submit(m_TextureShader, m_VertexArray, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));
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
    // std::shared_ptr<gl::Shader> m_Shader;
    std::shared_ptr<gl::Shader> m_TextureShader;
    std::shared_ptr<gl::Texture2D> m_Texture;
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
