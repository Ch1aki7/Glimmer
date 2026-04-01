// SandboxApp.cpp
#include <Glimmer.h>
#include "Platform/OpenGL/OpenGLShader.h"
#include <glm/gtc/type_ptr.hpp>
class ExampleLayer : public gl::Layer {
public:
    ExampleLayer() :Layer("Example"), m_Camera(-1.6f, 1.6f, -0.9f, 0.9f)
    {
		/*VAO设置*/
        m_VertexArray.reset(gl::VertexArray::Create());
		// 背景漩涡VAO
		m_bg_vortexVertexArray.reset(gl::VertexArray::Create());

        float vertices[4 * 5] = {
            // X, Y, Z          // U, V (0-1范围)
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, // 左下
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f, // 右下
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f, // 右上
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f  // 左上
        };
		float bg_vortexVertices[4 * 5] = {
			// 位置 (X, Y, Z)      // UV (U, V)
			-1.6f, -0.9f, 0.0f,   0.0f, 0.0f,
			 1.6f, -0.9f, 0.0f,   1.0f, 0.0f,
			 1.6f,  0.9f, 0.0f,   1.0f, 1.0f,
			-1.6f,  0.9f, 0.0f,   0.0f, 1.0f
		};

		/*VBO设置*/
        std::shared_ptr<gl::VertexBuffer> vertexBuffer;
        vertexBuffer.reset(gl::VertexBuffer::Create(vertices, sizeof(vertices)));
        vertexBuffer->SetLayout({
            { gl::ShaderDataType::Float3, "a_Position" },
            { gl::ShaderDataType::Float2, "a_TexCoord" }
            });
        m_VertexArray->AddVertexBuffer(vertexBuffer);
		// 背景漩涡VBO
		gl::Ref<gl::VertexBuffer> bg_VBO;
		bg_VBO.reset(gl::VertexBuffer::Create(bg_vortexVertices, sizeof(bg_vortexVertices)));
		bg_VBO->SetLayout({
			{ gl::ShaderDataType::Float3, "a_Position" },
			{ gl::ShaderDataType::Float2, "a_TexCoord" }
			});
		m_bg_vortexVertexArray->AddVertexBuffer(bg_VBO);

		/*IBO设置*/
        uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };
        std::shared_ptr<gl::IndexBuffer> indexBuffer;
        indexBuffer.reset(gl::IndexBuffer::Create(indices, 6));
        m_VertexArray->SetIndexBuffer(indexBuffer);
		// 背景漩涡IBO
		uint32_t bg_Indices[6] = { 0, 1, 2, 2, 3, 0 };
		gl::Ref<gl::IndexBuffer> bg_IBO;
		bg_IBO.reset(gl::IndexBuffer::Create(bg_Indices, 6));
		m_bg_vortexVertexArray->SetIndexBuffer(bg_IBO);

		/*路径设置*/
		m_ShaderLib.Load("assets/shaders/Texture.glsl");
		m_ShaderLib.Load("assets/shaders/BalatroVortex.glsl");
		m_ShaderLib.Load("assets/shaders/Tunnel.glsl");

		m_Texture = gl::Texture2D::Create("assets/textures/Balatro.png");
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

        float time = gl::Application::Get().GetTime();
		auto& window = gl::Application::Get().GetWindow();

		auto bgShader = m_ShaderLib.Get("BalatroVortex");
		bgShader->Bind();
		float vortexStrength = 2.0f + sin(time * 0.5f) * 1.5f;
		bgShader->UploadUniformFloat("u_VortexAmt", vortexStrength);
		bgShader->UploadUniformFloat("u_Time", time);
		bgShader->UploadUniformFloat2("u_Resolution", { (float)window.GetWidth(), (float)window.GetHeight() });
		gl::Renderer::Submit(bgShader, m_bg_vortexVertexArray, glm::mat4(1.0f));

		auto textureShader = m_ShaderLib.Get("Texture");
		textureShader->Bind();
		m_Texture->Bind();
		textureShader->UploadUniformFloat("u_Time", time);
		gl::Renderer::Submit(textureShader, m_VertexArray, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));

        gl::Renderer::EndScene();
    }

    void OnEvent(gl::Event& event) override {
        GL_TRACE("{0}", event.ToString());
    }

    void OnImGuiRender() override {
        ImGui::Begin("Glimmer Test Window");
        ImGui::Text("Hello World! ImGui is Working!");
        ImGui::End();

        bool show_demo_window = true;
        ImGui::ShowDemoWindow(&show_demo_window);
    }
private:
	gl::ShaderLibrary m_ShaderLib;

    std::shared_ptr<gl::VertexArray> m_VertexArray;
    std::shared_ptr<gl::VertexArray> m_bg_vortexVertexArray;

    std::shared_ptr<gl::Shader> m_TextureShader;
    std::shared_ptr<gl::Shader> m_bg_vortexShader;
	gl::Ref<gl::Shader> m_TunnelShader;

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
