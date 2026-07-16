#include "EditorLayer.h"
#include <glm/gtc/type_ptr.hpp>

namespace gl {

	EditorLayer::EditorLayer() :Layer("EditorLayer"), m_CameraController(1280.0f / 720.0f, true) {

	}

	void EditorLayer::OnAttach() {
		GL_PROFILE_FUNCTION();

		m_ShaderLib.Load("assets/shaders/BalatroVortex.glsl");
		m_ShaderLib.Load("assets/shaders/StarNest.glsl");
		m_Texture = Texture2D::Create("assets/textures/Balatro.png");
		m_STSTexture = Texture2D::Create("assets/textures/STS.png");
		m_HenryTexture = Texture2D::Create("assets/textures/Henry.jpg");

		m_3DShader = Shader::Create("assets/shaders/Model3D.glsl");

		// --- 加载所有 OBJ 模型 ---
		auto loadModel = [&](const std::string& path) {
			auto model = CreateRef<Model>(path);
			if (model) {
				m_Models.push_back(model);
				// 从路径提取文件名作为显示名称
				auto lastSlash = path.find_last_of("/\\");
				auto lastDot = path.rfind('.');
				auto start = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
				auto count = (lastDot == std::string::npos) ? path.size() - start : lastDot - start;
				m_ModelNames.push_back(path.substr(start, count));
			}
		};

		loadModel("assets/models/bunny.obj");
		loadModel("assets/models/dragon.obj");
		loadModel("assets/models/planet.obj");
		loadModel("assets/models/spacecraft.obj");
		loadModel("assets/models/suzanne.obj");

		GL_CORE_INFO("Loaded {0} models", m_Models.size());

		FramebufferSpecification fbSpec;
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_Framebuffer = Framebuffer::Create(fbSpec);
		m_PostProcessFB = Framebuffer::Create(fbSpec);

		m_ShaderLib.Load("assets/shaders/PostProcess.glsl");
		m_ShaderLib.Load("Phong", "assets/shaders/Phong.glsl");
		m_ShaderLib.Load("Toon", "assets/shaders/Toon.glsl");
		m_ShaderLib.Load("Blinn-Phong", "assets/shaders/BlinnPhong.glsl");
		m_ShaderLib.Load("Hologram", "assets/shaders/Hologram.glsl");
	}

	void EditorLayer::OnDetach() {
		GL_PROFILE_FUNCTION();

	}

	void EditorLayer::OnUpdate(Timestep ts) {
		GL_PROFILE_FUNCTION();

		m_CameraController.OnUpdate(ts);

		Renderer2D::ResetStats();
		{
			GL_PROFILE_SCOPE("Renderer Prep");
			m_Framebuffer->Bind();
			RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			RenderCommand::Clear();
		}

		{
			static float rotation = 0.0f;
			rotation += ts * 50.0f;

			GL_PROFILE_SCOPE("Renderer Draw");
			auto bgShader = m_ShaderLib.Get("BalatroVortex");
			Renderer2D::DrawFullscreenQuad(bgShader, 0.9f);

			// --- 3D 模型渲染 ---
			if (m_SelectedModelIndex >= 0 && m_SelectedModelIndex < (int)m_Models.size())
			{
				Renderer::BeginScene(m_CameraController.GetCamera());
				m_3DShader->Bind();

				m_3DShader->UploadUniformFloat3("u_LightPos", m_LightPos);
				m_3DShader->UploadUniformFloat3("u_LightColor", { 1.0f, 1.0f, 1.0f });
				m_3DShader->UploadUniformFloat3("u_ViewPos", m_CameraController.GetCamera().GetPosition());
				m_3DShader->UploadUniformInt("u_Texture", 0);

				auto& model = m_Models[m_SelectedModelIndex];
				glm::mat4 modelTransform = glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, 0.0f })
					* glm::rotate(glm::mat4(1.0f), glm::radians(-rotation), { 0, 1, 0 })
					* glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

				m_3DShader->UploadUniformMat4("u_Transform", modelTransform);
				model->Draw(m_3DShader, modelTransform);

				Renderer::EndScene();
			}

			// 2D 批处理渲染
			Renderer2D::BeginScene(m_CameraController.GetCamera());

			Renderer2D::DrawRotatedQuad({ 1.0f, -0.5f, -0.1f }, { 0.1f, 0.1f }, -rotation, { 1.0f, 1.0f, 1.0f, 1.0f });
			Renderer2D::DrawQuad({ 1.0f, -0.5f, -0.1f }, { 0.5f, 0.75f }, { 0.2f, 0.3f, 0.8f, 1.0f });
			Renderer2D::DrawQuad({ -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, m_Texture);
			Renderer2D::DrawRotatedQuad({ -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, rotation, m_Texture, 3, { 0.8f, 0.3f, 0.8f, 1.0f });
			Renderer2D::DrawQuad({ 1.0f, 0.0f, 0.0f }, { 2.0f, 1.0f }, m_STSTexture, 2);
			Renderer2D::DrawQuad({ 0.0f, 0.0f, 0.0f }, { 1.3f, 1.0f }, m_HenryTexture);

			Renderer2D::EndScene();

			m_Framebuffer->Unbind();

			if (m_PostProcessEnabled)
			{
				m_PostProcessFB->Bind();
				RenderCommand::Clear();

				auto grayscaleShader = m_ShaderLib.Get("PostProcess");

				Renderer2D::DrawPostProcess(grayscaleShader, m_Framebuffer->GetColorAttachmentRendererID());

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
				if (ImGui::MenuItem("Exit")) Application::Get().Close();
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		// 状态统计
		ImGui::Begin("Stats");
		auto stats = Renderer2D::GetStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %d", stats.DrawCalls);
		ImGui::Text("Quads: %d", stats.QuadCount);
		ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
		ImGui::Text("Indices: %d", stats.GetTotalIndexCount());
		ImGui::End();

		// Shader 选择
		ImGui::Begin("3D Settings");
		ImGui::Text("Select Lighting Model:");
		if (ImGui::Combo("Shader Type", &m_SelectedShaderIndex, m_ShaderNames, IM_ARRAYSIZE(m_ShaderNames)))
		{
			GL_CORE_INFO("Switched to Shader: {0}", m_ShaderNames[m_SelectedShaderIndex]);
			m_3DShader = m_ShaderLib.Get(m_ShaderNames[m_SelectedShaderIndex]);
		}

		ImGui::Separator();

		// 模型选择
		if (!m_ModelNames.empty())
		{
			// 构建 C 风格字符串数组兼容旧版 ImGui Combo
			std::vector<const char*> modelNameCStrs;
			for (auto& name : m_ModelNames)
				modelNameCStrs.push_back(name.c_str());

			ImGui::Text("Select Model:");
			if (ImGui::Combo("##ModelSelect", &m_SelectedModelIndex, modelNameCStrs.data(), (int)modelNameCStrs.size()))
			{
				GL_CORE_INFO("Switched to Model: {0}", m_ModelNames[m_SelectedModelIndex]);
			}
		}

		ImGui::Separator();

		// Shader 特有的动态调参
		if (m_SelectedShaderIndex == 3) // 全息(Hologram)
		{
			ImGui::Text("Hologram Settings");
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

	void EditorLayer::OnEvent(Event& event) {
		GL_TRACE("{0}", event.ToString());
		m_CameraController.OnEvent(event);
	}

}
