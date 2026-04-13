#include "Sandbox2D.h"
#include <glm/gtc/type_ptr.hpp>
Sandbox2D::Sandbox2D() :Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f, true) {

}

void Sandbox2D::OnAttach() {
	///*VAO设置*/
	//m_VertexArray = gl::VertexArray::Create();
	//// 背景漩涡VAO
	//m_bg_vortexVertexArray = gl::VertexArray::Create();

	//float vertices[4 * 5] = {
	//	// X, Y, Z          // U, V (0-1范围)
	//	-0.5f, -0.5f, 0.0f,  0.0f, 0.0f, // 左下
	//	 0.5f, -0.5f, 0.0f,  1.0f, 0.0f, // 右下
	//	 0.5f,  0.5f, 0.0f,  1.0f, 1.0f, // 右上
	//	-0.5f,  0.5f, 0.0f,  0.0f, 1.0f  // 左上
	//};
	//float bg_vortexVertices[4 * 5] = {
	//	-1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
	//	 1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
	//	 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
	//	-1.0f,  1.0f, 0.0f,  0.0f, 1.0f
	//};

	///*VBO设置*/
	//gl::Ref<gl::VertexBuffer> vertexBuffer;
	//vertexBuffer.reset(gl::VertexBuffer::Create(vertices, sizeof(vertices)));
	//vertexBuffer->SetLayout({
	//	{ gl::ShaderDataType::Float3, "a_Position" },
	//	{ gl::ShaderDataType::Float2, "a_TexCoord" }
	//	});
	//m_VertexArray->AddVertexBuffer(vertexBuffer);
	//// 背景漩涡VBO
	//gl::Ref<gl::VertexBuffer> bg_VBO;
	//bg_VBO.reset(gl::VertexBuffer::Create(bg_vortexVertices, sizeof(bg_vortexVertices)));
	//bg_VBO->SetLayout({
	//	{ gl::ShaderDataType::Float3, "a_Position" },
	//	{ gl::ShaderDataType::Float2, "a_TexCoord" }
	//	});
	//m_bg_vortexVertexArray->AddVertexBuffer(bg_VBO);

	///*IBO设置*/
	//uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };
	//std::shared_ptr<gl::IndexBuffer> indexBuffer;
	//indexBuffer.reset(gl::IndexBuffer::Create(indices, 6));
	//m_VertexArray->SetIndexBuffer(indexBuffer);
	//// 背景漩涡IBO
	//uint32_t bg_Indices[6] = { 0, 1, 2, 2, 3, 0 };
	//gl::Ref<gl::IndexBuffer> bg_IBO;
	//bg_IBO.reset(gl::IndexBuffer::Create(bg_Indices, 6));
	//m_bg_vortexVertexArray->SetIndexBuffer(bg_IBO);

	///*路径设置*/
	//m_ShaderLib.Load("assets/shaders/Texture.glsl");
	//m_ShaderLib.Load("assets/shaders/BalatroVortex.glsl");
	//m_ShaderLib.Load("assets/shaders/Tunnel.glsl");

	m_Texture = gl::Texture2D::Create("assets/textures/Balatro.png");
}

void Sandbox2D::OnDetach() {

}

void Sandbox2D::OnUpdate(gl::Timestep ts) {
	m_CameraController.OnUpdate(ts);

	gl::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	gl::RenderCommand::Clear();

	gl::Renderer2D::BeginScene(m_CameraController.GetCamera());

	//float time = gl::Application::Get().GetTime();
	//auto& window = gl::Application::Get().GetWindow();
	//float zoom = m_CameraController.GetZoomLevel();

	//auto bgShader = m_ShaderLib.Get("BalatroVortex");
	//bgShader->Bind();
	//float vortexStrength = 2.0f + sin(time * 0.5f) * 1.5f;
	//bgShader->UploadUniformFloat("u_VortexAmt", vortexStrength);
	//bgShader->UploadUniformFloat("u_Time", time);
	//bgShader->UploadUniformFloat2("u_Resolution", { (float)window.GetWidth(), (float)window.GetHeight() });
	//gl::Renderer::Submit(bgShader, m_bg_vortexVertexArray, glm::mat4(1.0f));

	//auto textureShader = m_ShaderLib.Get("Texture");
	//textureShader->Bind();
	//m_Texture->Bind();
	//textureShader->UploadUniformFloat("u_Time", time);
	//gl::Renderer::Submit(textureShader, m_VertexArray, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));


	gl::Renderer2D::DrawQuad({ -1.0f, 0.0f }, { 0.8f, 0.8f }, { 0.8f, 0.2f, 0.3f, 1.0f });
	gl::Renderer2D::DrawQuad({ 0.5f, -0.5f }, { 0.5f, 0.75f }, { 0.2f, 0.3f, 0.8f, 1.0f });
	gl::Renderer2D::DrawQuad({ 0.0f, 0.0f, -0.1f }, { 1.0f, 1.0f }, m_Texture); // 背景图

	gl::Renderer2D::EndScene();
}

void Sandbox2D::OnImGuiRender() {
	ImGui::Begin("Glimmer Test Window");
	ImGui::Text("Hello World! ImGui is Working!");
	ImGui::End();

	bool show_demo_window = true;
	ImGui::ShowDemoWindow(&show_demo_window);
}

void Sandbox2D::OnEvent(gl::Event& event) {
	GL_TRACE("{0}", event.ToString());
	m_CameraController.OnEvent(event);
}
