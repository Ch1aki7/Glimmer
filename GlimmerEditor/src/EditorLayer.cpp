#include "EditorLayer.h"
#include <glm/gtc/type_ptr.hpp>
EditorLayer::EditorLayer() :Layer("EditorLayer"), m_CameraController(1280.0f / 720.0f, true) {

}

void EditorLayer::OnAttach() {
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
	//m_PhoneShader = gl::Shader::Create("assets/shaders/Phong.glsl");
	//m_ToonShader = gl::Shader::Create("assets/shaders/Toon.glsl");
	//m_BlinnPhongShader = gl::Shader::Create("assets/shaders/BlinnPhong.glsl");
	//m_HologramShader = gl::Shader::Create("assets/shaders/Hologram.glsl");

	//m_TestTexture = gl::Texture2D::Create("assets/models/penguin.png");
	m_TestTexture = gl::Texture2D::Create("assets/models/Final_Texture.png");
	m_GirlTexture = gl::Texture2D::Create("assets/models/girl.png");


	gl::FramebufferSpecification fbSpec;
	fbSpec.Width = 1280;
	fbSpec.Height = 720;
	m_Framebuffer = gl::Framebuffer::Create(fbSpec);
	m_PostProcessFB = gl::Framebuffer::Create(fbSpec);

	m_ShaderLib.Load("assets/shaders/PostProcess.glsl");
	m_ShaderLib.Load("Phong", "assets/shaders/Phong.glsl");
	m_ShaderLib.Load("Toon", "assets/shaders/Toon.glsl");
	m_ShaderLib.Load("Blinn-Phong", "assets/shaders/BlinnPhong.glsl");
	m_ShaderLib.Load("Hologram", "assets/shaders/Hologram.glsl");
	m_ShaderLib.Load("Normal", "assets/shaders/Normal.glsl");
	m_ShaderLib.Load("CrossHatch", "assets/shaders/CrossHatch.glsl");
	m_ShaderLib.Load("InkOutline", "assets/shaders/InkOutline.glsl");
}

void EditorLayer::OnDetach() {
	GL_PROFILE_FUNCTION();

}

void EditorLayer::OnUpdate(gl::Timestep ts) {
	GL_PROFILE_FUNCTION();

	if (m_ViewportFocused)
		m_CameraController.OnUpdate(ts);

	gl::Renderer2D::ResetStats();
	{
		GL_PROFILE_SCOPE("Renderer Prep");
		m_Framebuffer->Bind();
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
		gl::Renderer2D::DrawRotatedQuad({ -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, rotation, m_Texture, 3, { 0.8f, 0.3f, 0.8f, 1.0f });
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

		m_Framebuffer->Unbind();

		if (m_PostProcessEnabled)
		{
			m_PostProcessFB->Bind();
			gl::RenderCommand::Clear();

			auto grayscaleShader = m_ShaderLib.Get("PostProcess");

			gl::Renderer2D::DrawPostProcess(grayscaleShader, m_Framebuffer->GetColorAttachmentRendererID());

			m_PostProcessFB->Unbind();

			m_FinalSceneTexture = m_PostProcessFB->GetColorAttachmentRendererID();
		}
		else
		{
			m_FinalSceneTexture = m_Framebuffer->GetColorAttachmentRendererID();
		}
	}
}

void EditorLayer::OnImGuiRender() {
	GL_PROFILE_FUNCTION();

	static bool dockspaceOpen = true;
	static bool opt_fullscreen_persistant = true;
	bool opt_fullscreen = opt_fullscreen_persistant;
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

	// 设置窗口标志：无标题栏、无缩放、无移动、无遮挡、带菜单栏
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	if (opt_fullscreen)
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	}

	// 开启 DockSpace 窗口
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
	ImGui::PopStyleVar();

	if (opt_fullscreen)
		ImGui::PopStyleVar(2);

	// 真正的停靠空间核心
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
	}

	// 这里可以加引擎顶部的菜单栏（如 File, Edit）
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit")) gl::Application::Get().Close();
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	// 状态统计
	ImGui::Begin("Stats");
	auto stats = gl::Renderer2D::GetStats();
	ImGui::Text("Renderer2D Stats:");
	ImGui::Text("Draw Calls: %d", stats.DrawCalls);
	ImGui::Text("Quads: %d", stats.QuadCount);
	ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
	ImGui::Text("Indices: %d", stats.GetTotalIndexCount());
	ImGui::End();

	ImGui::Begin("3Dshader");
	ImGui::Text("Select Lighting Model:");
	// ImGui 下拉菜单组件
	// 参数：标签, 当前索引指针, 选项数组, 数组长度
	if (ImGui::Combo("Shader Type", &m_SelectedShaderIndex, m_ShaderNames, IM_ARRAYSIZE(m_ShaderNames)))
	{
		// 这里可以添加选中后的即时逻辑，例如打印日志
		GL_CORE_INFO("Switched to Shader: {0}", m_ShaderNames[m_SelectedShaderIndex]);
		m_3DShader = m_ShaderLib.Get(m_ShaderNames[m_SelectedShaderIndex]);
	}

	ImGui::Separator(); // 画一条分割线

	// 可以在这里放一些和 3D Shader 相关的动态调参
	if (m_SelectedShaderIndex == 3) // 如果选了全息(Hologram)
	{
		ImGui::Text("Hologram Settings");
		// 这里可以放一些特有的滑动条
	}
	ImGui::End();


	// 调试信息
	ImGui::Begin("Settings");
	ImGui::Checkbox("Enable Post-Processing", &m_PostProcessEnabled);
	ImGui::DragFloat3("Light Position", glm::value_ptr(m_LightPos), 0.1f);
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
	ImGui::End();

	// 游戏视口 (Viewport)
	ImGui::Begin("Viewport");

	m_ViewportFocused = ImGui::IsWindowFocused();
	m_ViewportHovered = ImGui::IsWindowHovered();
	gl::Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused || !m_ViewportHovered);

	ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	auto& spec = m_Framebuffer->GetSpecification();
	if (viewportPanelSize.x > 0.0f && viewportPanelSize.y > 0.0f &&
		(spec.Width != viewportPanelSize.x || spec.Height != viewportPanelSize.y))
	{
		m_Framebuffer->Resize((uint32_t)viewportPanelSize.x, (uint32_t)viewportPanelSize.y);
		m_PostProcessFB->Resize((uint32_t)viewportPanelSize.x, (uint32_t)viewportPanelSize.y);

		m_CameraController.OnResize(viewportPanelSize.x, viewportPanelSize.y);
	}
	uint32_t textureID = m_FinalSceneTexture;
	ImGui::Image((void*)(uintptr_t)textureID, ImVec2{ viewportPanelSize.x, viewportPanelSize.y }, { 0, 1 }, { 1, 0 });
	ImGui::End();

	ImGui::End();

	//bool show_demo_window = true;
	//ImGui::ShowDemoWindow(&show_demo_window);
}

void EditorLayer::OnEvent(gl::Event& event) {
	GL_TRACE("{0}", event.ToString());
	m_CameraController.OnEvent(event);
}
