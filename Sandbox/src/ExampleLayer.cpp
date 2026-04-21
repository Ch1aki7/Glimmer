#include "ExampleLayer.h"
#include <glm/gtc/type_ptr.hpp>
ExampleLayer::ExampleLayer() :Layer("ExampleLayer"), m_CameraController(1280.0f / 720.0f, true) {

}

void ExampleLayer::OnAttach() {
	GL_PROFILE_FUNCTION();

	///*路径设置*/
	//m_ShaderLib.Load("assets/shaders/Texture.glsl");
	//m_ShaderLib.Load("assets/shaders/Tunnel.glsl");

	m_ShaderLib.Load("assets/shaders/BalatroVortex.glsl");
	m_Texture = gl::Texture2D::Create("assets/textures/Balatro.png");
	m_STSTexture = gl::Texture2D::Create("assets/textures/STS.png");
	m_HenryTexture = gl::Texture2D::Create("assets/textures/Henry.jpg");
}

void ExampleLayer::OnDetach() {
	GL_PROFILE_FUNCTION();

}

void ExampleLayer::OnUpdate(gl::Timestep ts) {
	GL_PROFILE_FUNCTION();

	m_CameraController.OnUpdate(ts);

	{
		GL_PROFILE_SCOPE("Renderer Prep");
		gl::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		gl::RenderCommand::Clear();
	}

	{
		static float rotation = 0.0f;
		rotation += ts * 50.0f;

		GL_PROFILE_SCOPE("Renderer Draw");
		auto bgShader = m_ShaderLib.Get("BalatroVortex");
		gl::Renderer2D::DrawFullscreenQuad(bgShader, 0.9f);

		gl::Renderer2D::BeginScene(m_CameraController.GetCamera());

		gl::Renderer2D::DrawRotatedQuad({ 1.0f, -0.5f, -0.1f }, { 0.1f, 0.1f }, -rotation, { 1.0f, 1.0f, 1.0f, 1.0f });
		gl::Renderer2D::DrawQuad({ 1.0f, -0.5f, -0.1f }, { 0.5f, 0.75f }, { 0.2f, 0.3f, 0.8f, 1.0f });
		gl::Renderer2D::DrawQuad({ -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, m_Texture);
		gl::Renderer2D::DrawRotatedQuad({ -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, rotation, m_Texture, 3, { 0.8f, 0.3f, 0.8f, 1.0f });
		gl::Renderer2D::DrawQuad({ 1.0f, 0.0f, 0.0f }, { 2.0f, 1.0f }, m_STSTexture, 2);
		gl::Renderer2D::DrawQuad({ 0.0f, 0.0f, 0.0f }, { 1.3f, 1.0f }, m_HenryTexture);

		gl::Renderer2D::EndScene();

	}
}

void ExampleLayer::OnImGuiRender() {
	GL_PROFILE_FUNCTION();

	ImGui::Begin("Glimmer Test Window");
	ImGui::Text("Hello World! ImGui is Working!");
	ImGui::End();

	bool show_demo_window = true;
	ImGui::ShowDemoWindow(&show_demo_window);
}

void ExampleLayer::OnEvent(gl::Event& event) {
	GL_TRACE("{0}", event.ToString());
	m_CameraController.OnEvent(event);
}
