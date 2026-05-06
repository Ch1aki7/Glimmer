#include "Sandbox2D.h"
#include <glm/gtc/type_ptr.hpp>
Sandbox2D::Sandbox2D() :Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f, true) {

}

void Sandbox2D::OnAttach() {
	GL_PROFILE_FUNCTION();

	///*路径设置*/
	//m_ShaderLib.Load("assets/shaders/Texture.glsl");
	//m_ShaderLib.Load("assets/shaders/Tunnel.glsl");

	m_ShaderLib.Load("assets/shaders/BalatroVortex.glsl");
	m_ShaderLib.Load("assets/shaders/StarNest.glsl");
	m_Texture = gl::Texture2D::Create("assets/textures/Balatro.png");
	m_STSTexture = gl::Texture2D::Create("assets/textures/STS.png");
	m_HenryTexture = gl::Texture2D::Create("assets/textures/Henry.jpg");

	//m_MeshModel = gl::CreateRef<gl::Model>("assets/models/penguin.obj");
	m_MeshModel = gl::CreateRef<gl::Model>("assets/models/企鹅高松灯.obj");
	m_ChairModel = gl::CreateRef<gl::Model>("assets/models/chair.obj");
	m_GirlModel = gl::CreateRef<gl::Model>("assets/models/girl.obj");
	m_3DShader = gl::Shader::Create("assets/shaders/Model3D.glsl");

	//m_TestTexture = gl::Texture2D::Create("assets/models/penguin.png");
	m_TestTexture = gl::Texture2D::Create("assets/models/Final_Texture.png");
	m_GirlTexture = gl::Texture2D::Create("assets/models/girl.png");
}

void Sandbox2D::OnDetach() {
	GL_PROFILE_FUNCTION();

}

void Sandbox2D::OnUpdate(gl::Timestep ts) {
	GL_PROFILE_FUNCTION();

	m_CameraController.OnUpdate(ts);

	gl::Renderer2D::ResetStats();
	{
		GL_PROFILE_SCOPE("Renderer Prep");
		gl::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		gl::RenderCommand::Clear();
	}

	{
		static float rotation = 0.0f;
		rotation += ts * 50.0f;

		GL_PROFILE_SCOPE("Renderer Draw");
		//auto bgShader = m_ShaderLib.Get("BalatroVortex");
		//gl::Renderer2D::DrawFullscreenQuad(bgShader, 0.9f);

		auto stShader = m_ShaderLib.Get("StarNest");
		gl::Renderer2D::DrawFullscreenQuad(stShader, 0.9f);

		// 3D obj渲染
		gl::Renderer::BeginScene(m_CameraController.GetCamera());
		m_3DShader->Bind();

		// --- 统一上传光照全局参数 (只需上传一次，所有 3D 模型通用) ---
		m_3DShader->UploadUniformFloat3("u_LightPos", m_LightPos);
		m_3DShader->UploadUniformFloat3("u_LightColor", { 1.0f, 1.0f, 1.0f });
		m_3DShader->UploadUniformFloat3("u_ViewPos", m_CameraController.GetCamera().GetPosition());
		// 显式告诉 3D Shader 去 0 号插槽找图
		m_3DShader->UploadUniformInt("u_Texture", 0);

		// --- 绘制企鹅 ---
		glm::mat4 penguinTransform = glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, 0.0f })
			* glm::rotate(glm::mat4(1.0f), glm::radians(-rotation), { 0, 1, 0 })
			* glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
		m_TestTexture->Bind(0); // 确保绑定到 0
		m_MeshModel->Draw(m_3DShader, penguinTransform);

		// --- 绘制椅子 (给它一张默认贴图，防止变黑) ---
		glm::mat4 chairTransform = glm::translate(glm::mat4(1.0f), { 1.0f, 1.0f, 0.0f })
			* glm::rotate(glm::mat4(1.0f), glm::radians(rotation), { 0, 1, 0 })
			* glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
		// 这里可以使用引擎的白贴图，或者任何通用贴图
		//gl::Renderer2D::GetWhiteTexture()->Bind(0);
		m_ChairModel->Draw(m_3DShader, chairTransform);

		// --- 绘制女孩 ---
		glm::mat4 girlTransform = glm::translate(glm::mat4(1.0f), { -1.0f, -1.0f, 0.1f })
			* glm::rotate(glm::mat4(1.0f), glm::radians(rotation), { 0, 1, 0 })
			* glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
		m_GirlTexture->Bind(0);
		m_GirlModel->Draw(m_3DShader, girlTransform);

		gl::Renderer::EndScene();

		// 2D 批处理渲染
		gl::Renderer2D::BeginScene(m_CameraController.GetCamera());
		
		gl::Renderer2D::DrawRotatedQuad({ 1.0f, -0.5f, -0.1f }, { 0.1f, 0.1f }, -rotation, { 1.0f, 1.0f, 1.0f, 1.0f });
		gl::Renderer2D::DrawQuad({ 1.0f, -0.5f, -0.1f }, { 0.5f, 0.75f }, { 0.2f, 0.3f, 0.8f, 1.0f });
		gl::Renderer2D::DrawQuad({ -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, m_Texture);
		gl::Renderer2D::DrawRotatedQuad({ -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, rotation, m_Texture, 3, {0.8f, 0.3f, 0.8f, 1.0f });
		gl::Renderer2D::DrawQuad({ 1.0f, 0.0f, 0.0f }, { 2.0f, 1.0f }, m_STSTexture, 2);
		gl::Renderer2D::DrawQuad({ 0.0f, 0.0f, 0.0f }, { 1.3f, 1.0f }, m_HenryTexture);

		gl::Renderer2D::EndScene();

		// 渲染统计测试
		//gl::Renderer2D::BeginScene(m_CameraController.GetCamera());
		//for (float y = -5.0f; y < 5.0f; y += 0.5f)
		//{
		//	for (float x = -5.0f; x < 5.0f; x += 0.5f)
		//	{
		//		glm::vec4 color = { (x + 5.0f) / 10.0f, 0.4f, (y + 5.0f) / 10.0f, 0.7f };
		//		gl::Renderer2D::DrawQuad({ x, y }, { 0.45f, 0.45f }, color);
		//	}
		//}
		//gl::Renderer2D::EndScene();

	}
}

void Sandbox2D::OnImGuiRender() {
	GL_PROFILE_FUNCTION();

	ImGui::Begin("Glimmer Test Window");
	ImGui::Text("Hello World! ImGui is Working!");
	ImGui::DragFloat3("Light Position", glm::value_ptr(m_LightPos), 0.1f);

	auto stats = gl::Renderer2D::GetStats();
	ImGui::Text("Renderer2D Stats:");
	ImGui::Text("Draw Calls: %d", stats.DrawCalls);
	ImGui::Text("Quads: %d", stats.QuadCount);
	ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
	ImGui::Text("Indices: %d", stats.GetTotalIndexCount());

	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
	ImGui::End();

	bool show_demo_window = true;
	ImGui::ShowDemoWindow(&show_demo_window);
}

void Sandbox2D::OnEvent(gl::Event& event) {
	GL_TRACE("{0}", event.ToString());
	m_CameraController.OnEvent(event);
}
