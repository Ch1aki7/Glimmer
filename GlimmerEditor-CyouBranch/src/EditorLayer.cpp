#include "EditorLayer.h"
#include "Glimmer/Scene/SceneSerializer.h"
#include "Glimmer/Utils/FileDialog.h"
#include "Glimmer/Core/Input.h"
#include <glm/gtc/type_ptr.hpp>
#include <ImGuizmo.h>

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

		// 白贴图：修复 DrawIndexed 后 slot 0 被解绑导致无贴图 3D 模型全黑
		m_WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whitePixel = 0xffffffff;
		m_WhiteTexture->SetData(&whitePixel, sizeof(uint32_t));

		m_3DShader = Shader::Create("assets/shaders/Model3D.glsl");

		// --- 加载所有 OBJ 模型 ---
		auto loadModel = [&](const std::string& path) {
			auto model = CreateRef<Model>(path);
			if (model) {
				m_Models.push_back(model);
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

		// ============================================================
		// 场景 & 层级面板测试
		// ============================================================
		m_ActiveScene = CreateRef<Scene>();

		// --- 测试实体 1: 主相机 (透视，原点，看向 -Z) ---
		auto cameraEntity = m_ActiveScene->CreateEntity("Main Camera");
		auto& camComp = cameraEntity.AddComponent<CameraComponent>();
		camComp.Camera.SetPerspective(glm::radians(45.0f), 0.1f, 100.0f);
		camComp.Camera.SetViewportSize(1280, 720);
		// 相机在原点，view = inverse(identity) = identity，实体在 z < 0 即为前方

		// --- 测试实体 2~4: 彩色方块 (z < 0，透视前方) ---
		auto redSquare = m_ActiveScene->CreateEntity("Red Square");
		redSquare.AddComponent<SpriteRendererComponent>(glm::vec4{ 1.0f, 0.2f, 0.2f, 1.0f });
		redSquare.GetComponent<TransformComponent>().Translation = { -2.0f, 1.0f, -3.0f };

		auto greenSquare = m_ActiveScene->CreateEntity("Green Square");
		greenSquare.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.2f, 1.0f, 0.2f, 1.0f });
		greenSquare.GetComponent<TransformComponent>().Translation = { 0.0f, 0.0f, -3.0f };

		auto blueSquare = m_ActiveScene->CreateEntity("Blue Square");
		blueSquare.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.2f, 0.2f, 1.0f, 1.0f });
		blueSquare.GetComponent<TransformComponent>().Translation = { 2.0f, -1.0f, -3.1f };

		// --- 测试实体 5: 无渲染组件的纯逻辑实体 ---
		auto logicNode = m_ActiveScene->CreateEntity("Logic Controller");
		// 仅 Tag + Transform，无 Sprite/Camera/Script，验证面板 badges

		// --- 初始化层级面板 ---
		m_HierarchyPanel.SetContext(m_ActiveScene);
		m_HierarchyPanel.OnEntitySelected = [&](Entity e) {
			if (e && e.HasComponent<TagComponent>())
				GL_CORE_TRACE("Hierarchy selected: {0}", e.GetComponent<TagComponent>().Tag);
		};
		m_HierarchyPanel.OnEntityDeleted = [&](Entity e) {
			if (e && e.HasComponent<TagComponent>())
				GL_CORE_TRACE("Hierarchy deleted: {0}", e.GetComponent<TagComponent>().Tag);
		};
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

		// Viewport resize 同步给场景
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f)
		{
			m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		}

		{
			static float rotation = 0.0f;
			rotation += ts * 50.0f;

			GL_PROFILE_SCOPE("Renderer Draw");
			//auto bgShader = m_ShaderLib.Get("BalatroVortex");
			//Renderer2D::DrawFullscreenQuad(bgShader, 0.9f);

			// --- 3D 模型渲染 ---
			//if (m_SelectedModelIndex >= 0 && m_SelectedModelIndex < (int)m_Models.size())
			//{
			//	// 确保 slot 0 有白贴图（修复 DrawIndexed 解绑导致的黑色问题）
			//	m_WhiteTexture->Bind(0);

			//	Renderer::BeginScene(m_CameraController.GetCamera());
			//	m_3DShader->Bind();

			//	m_3DShader->UploadUniformFloat3("u_LightPos", m_LightPos);
			//	m_3DShader->UploadUniformFloat3("u_LightColor", { 1.0f, 1.0f, 1.0f });
			//	m_3DShader->UploadUniformFloat3("u_ViewPos", m_CameraController.GetCamera().GetPosition());
			//	m_3DShader->UploadUniformInt("u_Texture", 0);

			//	auto& model = m_Models[m_SelectedModelIndex];
			//	glm::mat4 modelTransform = glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, 0.0f })
			//		* glm::rotate(glm::mat4(1.0f), glm::radians(-rotation), { 0, 1, 0 })
			//		* glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

			//	m_3DShader->UploadUniformMat4("u_Transform", modelTransform);
			//	model->Draw(m_3DShader, modelTransform);

			//	Renderer::EndScene();
			//}

			// --- 场景 ECS 渲染（遍历 Sprite 实体） ---
			m_ActiveScene->OnUpdateRuntime(ts);

			// 2D 批处理渲染
			//Renderer2D::BeginScene(m_CameraController.GetCamera());

			//Renderer2D::DrawRotatedQuad({ 1.0f, -0.5f, -0.1f }, { 0.1f, 0.1f }, -rotation, { 1.0f, 1.0f, 1.0f, 1.0f });
			//Renderer2D::DrawQuad({ 1.0f, -0.5f, -0.1f }, { 0.5f, 0.75f }, { 0.2f, 0.3f, 0.8f, 1.0f });
			//Renderer2D::DrawQuad({ -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, m_Texture);
			//Renderer2D::DrawRotatedQuad({ -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, rotation, m_Texture, 3, { 0.8f, 0.3f, 0.8f, 1.0f });
			//Renderer2D::DrawQuad({ 1.0f, 0.0f, 0.0f }, { 2.0f, 1.0f }, m_STSTexture, 2);
			//Renderer2D::DrawQuad({ 0.0f, 0.0f, 0.0f }, { 1.3f, 1.0f }, m_HenryTexture);

			//Renderer2D::EndScene();

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

		// --- 全局快捷键（独立于菜单焦点） ---
		auto& io = ImGui::GetIO();
		if (ImGui::IsKeyChordPressed(ImGuiKey_N | ImGuiMod_Ctrl)) {
			m_ActiveScene = CreateRef<Scene>();
			m_HierarchyPanel.SetContext(m_ActiveScene);
			m_HierarchyPanel.SetSelectedEntity({});
		}
		if (ImGui::IsKeyChordPressed(ImGuiKey_S | ImGuiMod_Ctrl)) {
			std::string path = FileDialog::SaveFile("Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
			if (!path.empty()) {
				SceneSerializer serializer(m_ActiveScene);
				serializer.Serialize(path);
				GL_CORE_INFO("Scene saved to {0}", path);
			}
		}
		if (ImGui::IsKeyChordPressed(ImGuiKey_O | ImGuiMod_Ctrl)) {
			std::string path = FileDialog::OpenFile("Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
			if (!path.empty()) {
				auto newScene = CreateRef<Scene>();
				SceneSerializer serializer(newScene);
				if (serializer.Deserialize(path)) {
					m_ActiveScene = newScene;
					m_HierarchyPanel.SetContext(m_ActiveScene);
					m_HierarchyPanel.SetSelectedEntity({});
				}
			}
		}

		// --- Gizmo 快捷键 (1/2/3 切换变换模式) ---
		if (m_ViewportHovered) {
			if (ImGui::IsKeyPressed(ImGuiKey_1)) m_GizmoType = 0;
			if (ImGui::IsKeyPressed(ImGuiKey_2)) m_GizmoType = 1;
			if (ImGui::IsKeyPressed(ImGuiKey_3)) m_GizmoType = 2;
		}

		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

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

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("New", "Ctrl+N"))
					{
						m_ActiveScene = CreateRef<Scene>();
						m_HierarchyPanel.SetContext(m_ActiveScene);
						m_HierarchyPanel.SetSelectedEntity({});
					}
					if (ImGui::MenuItem("Save As...", "Ctrl+S"))
					{
						std::string path = FileDialog::SaveFile("Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
						if (!path.empty()) {
							SceneSerializer serializer(m_ActiveScene);
							serializer.Serialize(path);
							GL_CORE_INFO("Scene saved to {0}", path);
						}
					}
					if (ImGui::MenuItem("Open...", "Ctrl+O"))
					{
						std::string path = FileDialog::OpenFile("Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
						if (!path.empty()) {
							auto newScene = CreateRef<Scene>();
							SceneSerializer serializer(newScene);
							if (serializer.Deserialize(path))
							{
								m_ActiveScene = newScene;
								m_HierarchyPanel.SetContext(m_ActiveScene);
								m_HierarchyPanel.SetSelectedEntity({});
							}
						}
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Exit")) Application::Get().Close();
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

		// ============================================================
		// Scene Hierarchy 面板（低耦合：仅通过回调通信）
		// ============================================================
		m_HierarchyPanel.OnImGuiRender();

		// Stats
		ImGui::Begin("Stats");
		auto stats = Renderer2D::GetStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %d", stats.DrawCalls);
		ImGui::Text("Quads: %d", stats.QuadCount);
		ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
		ImGui::Text("Indices: %d", stats.GetTotalIndexCount());
		ImGui::End();

		// 3D Settings
		ImGui::Begin("3D Settings");
		ImGui::Text("Select Lighting Model:");
		if (ImGui::Combo("Shader Type", &m_SelectedShaderIndex, m_ShaderNames, IM_ARRAYSIZE(m_ShaderNames)))
		{
			GL_CORE_INFO("Switched to Shader: {0}", m_ShaderNames[m_SelectedShaderIndex]);
			m_3DShader = m_ShaderLib.Get(m_ShaderNames[m_SelectedShaderIndex]);
		}

		ImGui::Separator();

		if (!m_ModelNames.empty())
		{
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
		{
			const char* gizmoNames[] = { "Translate (W)", "Rotate (E)", "Scale (R)" };
			ImGui::Combo("Gizmo", &m_GizmoType, gizmoNames, 3);
		}

		if (m_SelectedShaderIndex == 3)
		{
			ImGui::Text("Hologram Settings");
		}
		ImGui::End();

		// Settings
		ImGui::Begin("Settings");
		ImGui::Checkbox("Enable Post-Processing", &m_PostProcessEnabled);
		ImGui::DragFloat3("Light Position", glm::value_ptr(m_LightPos), 0.1f);
		ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
		ImGui::End();

		// Viewport
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();

		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();
		m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
		m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

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

		// --- ImGuizmo 变换控件 ---
		Entity selectedEntity = m_HierarchyPanel.GetSelectedEntity();
		if (selectedEntity && selectedEntity.HasComponent<TransformComponent>())
		{
			Entity camEntity = m_ActiveScene->GetPrimaryCameraEntity();
			auto& ct = camEntity.GetComponent<TransformComponent>();
			auto& cc = camEntity.GetComponent<CameraComponent>();
			glm::mat4 view  = glm::inverse(ct.GetTransform());
			glm::mat4 proj  = cc.Camera.GetProjection();

			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
			ImGuizmo::SetRect(
				m_ViewportBounds[0].x, m_ViewportBounds[0].y,
				m_ViewportBounds[1].x - m_ViewportBounds[0].x,
				m_ViewportBounds[1].y - m_ViewportBounds[0].y);

			auto& tc = selectedEntity.GetComponent<TransformComponent>();
			glm::mat4 transform = tc.GetTransform();

			bool snap = Input::IsKeyPressed(GL_KEY_LEFT_CONTROL);
			float snapVal = (m_GizmoType == 1) ? 45.0f : 0.5f;
			float snapValues[3] = { snapVal, snapVal, snapVal };

			ImGuizmo::OPERATION op = (m_GizmoType == 1) ? ImGuizmo::ROTATE
			                         : (m_GizmoType == 2) ? ImGuizmo::SCALE
			                         : ImGuizmo::TRANSLATE;

			ImGuizmo::Manipulate(
				glm::value_ptr(view), glm::value_ptr(proj),
				op, ImGuizmo::LOCAL,
				glm::value_ptr(transform),
				nullptr, snap ? snapValues : nullptr);

			if (ImGuizmo::IsUsing())
			{
				float t[3], r[3], s[3];
				ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), t, r, s);

				tc.Translation = { t[0], t[1], t[2] };
				tc.Rotation += glm::vec3(r[0], r[1], r[2]) - tc.Rotation;
				tc.Scale = { s[0], s[1], s[2] };
			}
		}

		ImGui::End();
		ImGui::PopStyleVar();

		ImGui::End();
	}

	void EditorLayer::OnEvent(Event& event) {
		GL_TRACE("{0}", event.ToString());

		// 当 ImGui 需要键盘时（正在输入文本框中），不转发给相机
		if (event.IsInCategory(EventCategoryKeyboard)) {
			if (ImGui::GetIO().WantCaptureKeyboard)
				return;
		}

		// 仅当视口悬停时才把事件给相机
		// （点击 UI 面板时不应拖动场景）
		if (event.IsInCategory(EventCategoryMouse)) {
			if (!m_ViewportHovered)
				return;
		}

		m_CameraController.OnEvent(event);
	}

}
