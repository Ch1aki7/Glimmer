#include "EditorLayer.h"
#include "Glimmer/Scene/SceneSerializer.h"
#include "Glimmer/Utils/FileDialog.h"
#include "Glimmer/Core/Input.h"
#include "Glimmer/Renderer/RenderPass.h"
#include <glm/gtc/type_ptr.hpp>
#include <ImGuizmo.h>

#include <algorithm>
#include <vector>
namespace gl {

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"),
		  m_ShaderPanel(&m_ShaderLib),
		  m_EditorCamera(45.0f, 1280.0f / 720.0f) {
	}

	void EditorLayer::SetEditorScene(const Ref<Scene>& scene)
	{
		GL_CORE_ASSERT(scene, "Editor scene cannot be null.");
		if (m_SceneState == SceneState::Play)
			OnSceneStop();

		m_EditorScene = scene;
		m_ActiveScene = m_EditorScene;
		m_HierarchyPanel.SetContext(m_ActiveScene);
		m_HierarchyPanel.SetSelectedEntity({});
	}

	void EditorLayer::OnScenePlay()
	{
		if (m_SceneState != SceneState::Edit || !m_EditorScene)
			return;

		UUID selectedUUID(0);
		Entity selectedEntity = m_HierarchyPanel.GetSelectedEntity();
		if (selectedEntity && selectedEntity.HasComponent<IDComponent>())
			selectedUUID = selectedEntity.GetUUID();

		m_RuntimeScene = Scene::Copy(m_EditorScene);
		m_ActiveScene = m_RuntimeScene;
		m_ActiveScene->OnRuntimeStart();
		m_SceneState = SceneState::Play;
		GL_CORE_INFO("Runtime scene started.");

		m_HierarchyPanel.SetContext(m_ActiveScene);
		m_HierarchyPanel.SetSelectedEntity(
			static_cast<uint64_t>(selectedUUID) != 0
				? m_ActiveScene->FindEntityByUUID(selectedUUID)
				: Entity{});
	}

	void EditorLayer::OnSceneStop()
	{
		if (m_SceneState != SceneState::Play)
			return;

		UUID selectedUUID(0);
		Entity selectedEntity = m_HierarchyPanel.GetSelectedEntity();
		if (selectedEntity && selectedEntity.HasComponent<IDComponent>())
			selectedUUID = selectedEntity.GetUUID();

		if (m_RuntimeScene)
			m_RuntimeScene->OnRuntimeStop();

		m_ActiveScene = m_EditorScene;
		m_RuntimeScene.reset();
		m_SceneState = SceneState::Edit;
		GL_CORE_INFO("Runtime scene stopped; editor scene restored.");

		m_HierarchyPanel.SetContext(m_ActiveScene);
		m_HierarchyPanel.SetSelectedEntity(
			m_ActiveScene && static_cast<uint64_t>(selectedUUID) != 0
				? m_ActiveScene->FindEntityByUUID(selectedUUID)
				: Entity{});
	}
	void EditorLayer::OnAttach() {
		GL_PROFILE_FUNCTION();
		AssetManager::Initialize("assets");

		m_ShaderLib.Load("assets/shaders/BalatroVortex.glsl");
		m_ShaderLib.Load("assets/shaders/StarNest.glsl");
		m_Texture = AssetManager::GetTexture2D(
			AssetManager::ImportAsset("assets/textures/Balatro.png"));
		m_STSTexture = AssetManager::GetTexture2D(
			AssetManager::ImportAsset("assets/textures/STS.png"));
		m_HenryTexture = AssetManager::GetTexture2D(
			AssetManager::ImportAsset("assets/textures/Henry.jpg"));
		const AssetHandle defaultMaterialHandle =
			AssetManager::ImportAsset("assets/materials/DefaultSprite.glmat");
		AssetManager::GetMaterial(defaultMaterialHandle);
		const AssetHandle pbrShaderHandle =
			AssetManager::ImportAsset("assets/shaders/PBRModel.glsl");
		const AssetHandle defaultPBRMaterialHandle =
			AssetManager::ImportAsset("assets/materials/DefaultPBR.glmat");
		const AssetHandle suzanneModelHandle =
			AssetManager::ImportAsset("assets/models/suzanne.obj");

		if (Ref<Material> material = AssetManager::GetMaterial(defaultPBRMaterialHandle))
		{
			if (material->GetShaderHandle() != pbrShaderHandle)
			{
				material->SetShaderHandle(pbrShaderHandle);
				material->Save();
			}
		}

		m_WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whitePixel = 0xffffffff;
		m_WhiteTexture->SetData(&whitePixel, sizeof(uint32_t));

		// --- 地形系统 ---
		m_TerrainShader = m_ShaderLib.Load("Terrain", "assets/shaders/Terrain.glsl");
		
		// 加载预设高度图
		
		m_HeightMapTexture = Texture2D::Create("assets/textures/heightmap-example.png");

		SimulationGridSpecification heightGridSpecification;
		heightGridSpecification.Width = 512;
		heightGridSpecification.Height = 512;
		heightGridSpecification.Format = TextureFormat::R32F;
		heightGridSpecification.Filter = TextureFilter::Linear;
		heightGridSpecification.Wrap = TextureWrap::ClampToEdge;

		m_TerrainGenerator = CreateScope<TerrainGenerator>(
			heightGridSpecification,
			"assets/shaders/Terrain/GenerateFBM.comp");
		m_TerrainPanel.SetContext(m_TerrainGenerator.get());
		m_TerrainMesh = CreateRef<TerrainMesh>(256);
		FramebufferSpecification fbSpec;
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		fbSpec.Attachments = {
			{ FramebufferTextureFormat::RGBA8 },
			{ FramebufferTextureFormat::RED_INTEGER }  // 鼠标拾取
		};
		m_Framebuffer = Framebuffer::Create(fbSpec);
		m_PostProcessFB = Framebuffer::Create(fbSpec);

		m_ShaderLib.Load("assets/shaders/PostProcess.glsl");
		m_ShaderLib.Load("assets/shaders/Overlay.glsl");
		m_ShaderLib.Load("Phong", "assets/shaders/Phong.glsl");
		m_ShaderLib.Load("Toon", "assets/shaders/Toon.glsl");
		m_ShaderLib.Load("Blinn-Phong", "assets/shaders/BlinnPhong.glsl");
		m_ShaderLib.Load("Hologram", "assets/shaders/Hologram.glsl");

		// --- 场景 ---
		SetEditorScene(CreateRef<Scene>());

		// Play 模式需要的 ECS 主相机（带可视化标记 + Gizmo 交互）
		auto camEntity = m_ActiveScene->CreateEntity("Main Camera");
		auto& cc = camEntity.AddComponent<CameraComponent>();
		cc.Camera.SetPerspective(glm::radians(45.0f), 0.1f, 1000.0f);
		cc.Camera.SetViewportSize(1280, 720);
		camEntity.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.8f, 0.8f, 0.2f, 0.6f }); // 半透明黄色标记
		camEntity.GetComponent<TransformComponent>().Translation = { 0.0f, 0.0f, 0.0f };

		// 测试实体
		auto redSquare = m_ActiveScene->CreateEntity("Red Square");
		redSquare.AddComponent<SpriteRendererComponent>(glm::vec4{ 1.0f, 0.2f, 0.2f, 1.0f });
		redSquare.AddComponent<MaterialComponent>(defaultMaterialHandle);
		redSquare.GetComponent<TransformComponent>().Translation = { -2.0f, 1.0f, -3.0f };

		auto greenSquare = m_ActiveScene->CreateEntity("Green Square");
		greenSquare.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.2f, 1.0f, 0.2f, 1.0f });
		greenSquare.GetComponent<TransformComponent>().Translation = { 0.0f, 0.0f, -3.0f };

		auto blueSquare = m_ActiveScene->CreateEntity("Blue Square");
		blueSquare.AddComponent<SpriteRendererComponent>(glm::vec4{ 0.2f, 0.2f, 1.0f, 1.0f });
		blueSquare.GetComponent<TransformComponent>().Translation = { 2.0f, -1.0f, -3.1f };

		auto logicNode = m_ActiveScene->CreateEntity("Logic Controller");

		auto pbrModelEntity = m_ActiveScene->CreateEntity("PBR Suzanne");
		pbrModelEntity.AddComponent<ModelRendererComponent>(suzanneModelHandle);
		pbrModelEntity.AddComponent<MaterialComponent>(defaultPBRMaterialHandle);

		auto sunEntity = m_ActiveScene->CreateEntity("Sun");
		sunEntity.AddComponent<DirectionalLightComponent>();
		sunEntity.GetComponent<TransformComponent>().Rotation = { -50.0f, 30.0f, 0.0f };

		auto pointLightEntity = m_ActiveScene->CreateEntity("Point Light");
		auto& pointLight = pointLightEntity.AddComponent<PointLightComponent>();
		pointLight.Intensity = 80.0f;
		pointLight.Range = 40.0f;
		pointLightEntity.GetComponent<TransformComponent>().Translation = { 0.0f, 12.0f, 0.0f };

		// --- 层级面板 ---
		m_HierarchyPanel.SetContext(m_ActiveScene);
		m_HierarchyPanel.OnEntitySelected = [&](Entity e) {
			if (e && e.HasComponent<TagComponent>())
				GL_CORE_TRACE("Hierarchy selected: {0}", e.GetComponent<TagComponent>().Tag);
			};
		m_HierarchyPanel.OnEntityDeleted = [&](Entity e) {
			if (e && e.HasComponent<TagComponent>())
				GL_CORE_TRACE("Hierarchy deleted: {0}", e.GetComponent<TagComponent>().Tag);
			};

		// --- 内容浏览器 ---
		m_ContentBrowser.OnFileDoubleClicked = [&](const std::string& path) {
			auto ext = std::filesystem::path(path).extension().string();
			if (ext == ".glimmer") {
				auto newScene = CreateRef<Scene>();
				SceneSerializer serializer(newScene);
				if (serializer.Deserialize(path)) {
					SetEditorScene(newScene);
					GL_CORE_INFO("Loaded scene: {0}", path);
				}
			}
			};


	}
	void EditorLayer::OnDetach() {
		GL_PROFILE_FUNCTION();
		if (m_SceneState == SceneState::Play)
			OnSceneStop();
		AssetManager::Shutdown();
	}

	void EditorLayer::OnUpdate(Timestep ts) {
		GL_PROFILE_FUNCTION();
		m_ShaderPanel.OnUpdate();
		m_TerrainPanel.OnUpdate();

		// --- 编辑器相机（仅编辑模式） ---
		if (m_SceneState == SceneState::Edit)
			m_EditorCamera.OnUpdate(ts);

		Renderer2D::ResetStats();

		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f)
		{
			m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
			if (m_ActiveScene)
			{
				m_ActiveScene->OnViewportResize(
					static_cast<uint32_t>(m_ViewportSize.x),
					static_cast<uint32_t>(m_ViewportSize.y));
			}
		}

		RenderPassSpecification scenePass;
		scenePass.Target = m_Framebuffer;
		scenePass.ClearColorValue = { 0.1f, 0.1f, 0.1f, 1 };
		RenderPass::Begin(scenePass);
		m_Framebuffer->ClearAttachment(1, -1);

		{
			GL_PROFILE_SCOPE("Scene Draw");
			if (m_SceneState == SceneState::Edit)
			{
				glm::mat4 vp = m_EditorCamera.GetProjectionMatrix() * m_EditorCamera.GetViewMatrix();
				m_ActiveScene->OnUpdateEditor(ts, vp, m_EditorCamera.GetPosition());
			}
			else
			{
				m_ActiveScene->OnUpdateRuntime(ts);
			}
		}

		RenderPass::End();

		// --- Pass 2: Terrain ---
		{
			RenderPassSpecification terrainPass;
			terrainPass.Target = m_Framebuffer;
			terrainPass.ClearColor = false;
			terrainPass.ClearDepth = false;
			RenderPass::Begin(terrainPass);
			
			glm::mat4 vp = m_EditorCamera.GetProjectionMatrix() * m_EditorCamera.GetViewMatrix();
			const Ref<Texture2D>& activeHeightMap =
				m_UseProceduralTerrain ? m_TerrainGenerator->GetHeightMap() : m_HeightMapTexture;
			m_TerrainShader->Bind();
			m_TerrainShader->UploadUniformMat4("u_ViewProjection", vp);
			m_TerrainShader->UploadUniformFloat("u_MaxHeight", m_TerrainMaxHeight);
			m_TerrainShader->UploadUniformFloat("u_UVScale", 1.0f);
			activeHeightMap->Bind(0);
			m_TerrainShader->UploadUniformInt("u_HeightMap", 0);
			m_TerrainShader->UploadUniformFloat2("u_TexelSize", {
				1.0f / static_cast<float>(activeHeightMap->GetWidth()),
				1.0f / static_cast<float>(activeHeightMap->GetHeight())
			});
			m_TerrainShader->UploadUniformFloat("u_SampleSpacing",
				static_cast<float>(m_TerrainMesh->GetGridSize()) /
				static_cast<float>(std::max(activeHeightMap->GetWidth() - 1, 1u)));
			m_TerrainShader->UploadUniformFloat3("u_CameraPos", m_EditorCamera.GetPosition());
			m_TerrainMesh->Bind();
			RenderCommand::DrawIndexed(m_TerrainMesh->GetVertexArray(), m_TerrainMesh->GetIndexCount());
			
			RenderPass::End();
		}

		// --- Pass 3: Overlay ---
		//{
		//	RenderPassSpecification overlayPass;
		//	overlayPass.Target = m_Framebuffer;
		//	overlayPass.ClearColor = false;
		//	overlayPass.ClearDepth = false;
		//	RenderPass::Begin(overlayPass);
		//	auto overlay = m_ShaderLib.Get("Overlay");
		//	Renderer2D::DrawFullscreenQuad(overlay, 0.0f);
		//	RenderPass::End();
		//}

		if (m_PostProcessEnabled)
		{
			RenderPassSpecification ppPass;
			ppPass.Target = m_PostProcessFB;
			RenderPass::Begin(ppPass);
			auto grayscaleShader = m_ShaderLib.Get("PostProcess");
			Renderer2D::DrawPostProcess(grayscaleShader, m_Framebuffer->GetColorAttachmentRendererID());
			RenderPass::End();
			m_FinalSceneTexture = m_PostProcessFB->GetColorAttachmentRendererID();
		}
		else
		{
			m_FinalSceneTexture = m_Framebuffer->GetColorAttachmentRendererID();
		}
	}

	void EditorLayer::OnImGuiRender() {
		GL_PROFILE_FUNCTION();

		// --- 全局快捷键 ---
		auto& io = ImGui::GetIO();
		if (ImGui::IsKeyChordPressed(ImGuiKey_P | ImGuiMod_Ctrl)) {
			if (m_SceneState == SceneState::Play)
				OnSceneStop();
			else
				OnScenePlay();
		}
		if (ImGui::IsKeyChordPressed(ImGuiKey_N | ImGuiMod_Ctrl)) {
			SetEditorScene(CreateRef<Scene>());
		}
		if (ImGui::IsKeyChordPressed(ImGuiKey_S | ImGuiMod_Ctrl)) {
			std::string path = FileDialog::SaveFile("Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
			if (!path.empty()) {
				SceneSerializer serializer(m_EditorScene);
				serializer.Serialize(path);
			}
		}
		if (ImGui::IsKeyChordPressed(ImGuiKey_O | ImGuiMod_Ctrl)) {
			std::string path = FileDialog::OpenFile("Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
			if (!path.empty()) {
				auto newScene = CreateRef<Scene>();
				SceneSerializer serializer(newScene);
				if (serializer.Deserialize(path)) {
					SetEditorScene(newScene);
				}
			}
		}
		if (m_ViewportHovered) {
			if (ImGui::IsKeyPressed(ImGuiKey_1)) m_GizmoType = 0;
			if (ImGui::IsKeyPressed(ImGuiKey_2)) m_GizmoType = 1;
			if (ImGui::IsKeyPressed(ImGuiKey_3)) m_GizmoType = 2;
		}

		// --- DockSpace ---
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
					SetEditorScene(CreateRef<Scene>());
				}
				if (ImGui::MenuItem("Save As...", "Ctrl+S"))
				{
					std::string path = FileDialog::SaveFile("Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
					if (!path.empty()) {
						SceneSerializer serializer(m_EditorScene);
						serializer.Serialize(path);
					}
				}
				if (ImGui::MenuItem("Open...", "Ctrl+O"))
				{
					std::string path = FileDialog::OpenFile("Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
					if (!path.empty()) {
						auto newScene = CreateRef<Scene>();
						SceneSerializer serializer(newScene);
						if (serializer.Deserialize(path)) {
							SetEditorScene(newScene);
						}
					}
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Exit")) Application::Get().Close();
				ImGui::EndMenu();
			}
			// 播放/停止按钮（菜单栏右侧）
		bool isPlaying = (m_SceneState == SceneState::Play);
		ImGui::SameLine(ImGui::GetWindowWidth() - 60);
		if (isPlaying)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
			if (ImGui::Button("\xef\x81\x8d Stop")) // 
				OnSceneStop();
			ImGui::PopStyleColor();
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.1f, 1.0f));
			if (ImGui::Button("\xef\x81\x8b Play")) // 
				OnScenePlay();
			ImGui::PopStyleColor();
		}

		ImGui::EndMenuBar();
		}

		// --- Scene Hierarchy ---
		m_HierarchyPanel.OnImGuiRender();

		// --- Content Browser ---
		m_ContentBrowser.OnImGuiRender();
		m_ShaderPanel.OnImGuiRender();
		m_TerrainPanel.OnImGuiRender();

		// Stats
		ImGui::Begin("Stats");
		auto stats = Renderer2D::GetStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %d", stats.DrawCalls);
		ImGui::Text("Quads: %d", stats.QuadCount);
		ImGui::End();

		// 3D Settings
		ImGui::Begin("3D Settings");
		ImGui::TextWrapped(
			"3D rendering is entity-driven. Assign Model Renderer and Material "
			"components in the Properties panel.");
		ImGui::Separator();
		const char* gizmoNames[] = { "Translate", "Rotate", "Scale" };
		ImGui::Combo("Gizmo", &m_GizmoType, gizmoNames, 3);
		ImGui::End();

		// Settings
		ImGui::Begin("Settings");
		ImGui::Checkbox("Enable Post-Processing", &m_PostProcessEnabled);
		ImGui::SeparatorText("Terrain Test");
		ImGui::Checkbox("Use Procedural Height Map", &m_UseProceduralTerrain);
		ImGui::DragFloat("Terrain Max Height", &m_TerrainMaxHeight, 0.1f, 0.0f, 100.0f);
		ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SquareColor));
		ImGui::End();

		// --- Viewport ---
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();

		auto vpMin = ImGui::GetWindowContentRegionMin();
		auto vpMax = ImGui::GetWindowContentRegionMax();
		auto vpOff = ImGui::GetWindowPos();
		m_ViewportBounds[0] = { vpMin.x + vpOff.x, vpMin.y + vpOff.y };
		m_ViewportBounds[1] = { vpMax.x + vpOff.x, vpMax.y + vpOff.y };

		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };


		auto& spec = m_Framebuffer->GetSpecification();
		if (viewportPanelSize.x > 0.0f && viewportPanelSize.y > 0.0f &&
			(spec.Width != viewportPanelSize.x || spec.Height != viewportPanelSize.y))
		{
			m_Framebuffer->Resize((uint32_t)viewportPanelSize.x, (uint32_t)viewportPanelSize.y);
			m_PostProcessFB->Resize((uint32_t)viewportPanelSize.x, (uint32_t)viewportPanelSize.y);
			m_EditorCamera.SetViewportSize(viewportPanelSize.x, viewportPanelSize.y);
		}

		uint32_t textureID = m_FinalSceneTexture;
		ImGui::Image((void*)(uintptr_t)textureID, ImVec2{ viewportPanelSize.x, viewportPanelSize.y }, { 0, 1 }, { 1, 0 });

		if (ImGui::BeginDragDropTarget())
		{
			if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
			{
				std::string path((const char*)payload->Data, payload->DataSize - 1);
				auto ext = std::filesystem::path(path).extension().string();
				if (ext == ".glimmer")
				{
					auto newScene = CreateRef<Scene>();
					SceneSerializer serializer(newScene);
					if (serializer.Deserialize(path))
					{
						SetEditorScene(newScene);
						GL_CORE_INFO("Dropped scene: {0}", path);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}


		// --- Gizmos (仅编辑模式) ---
		if (m_SceneState == SceneState::Edit)
		{
		Entity selectedEntity = m_HierarchyPanel.GetSelectedEntity();
		if (selectedEntity && selectedEntity.HasComponent<TransformComponent>())
		{
			const glm::mat4& view = m_EditorCamera.GetViewMatrix();
			const glm::mat4& proj = m_EditorCamera.GetProjectionMatrix();

			// --- 相机可视范围 ---
			if (selectedEntity.HasComponent<CameraComponent>())
			{
				auto& cc = selectedEntity.GetComponent<CameraComponent>();
				auto& ct = selectedEntity.GetComponent<TransformComponent>();
				glm::mat4 camView = glm::inverse(ct.GetTransform());
				glm::mat4 camProj = cc.Camera.GetProjection();
				glm::mat4 invVP = glm::inverse(camProj * camView);
				glm::vec4 corners[8] = {
					{-1,-1,-1,1}, { 1,-1,-1,1}, { 1, 1,-1,1}, {-1, 1,-1,1},
					{-1,-1, 1,1}, { 1,-1, 1,1}, { 1, 1, 1,1}, {-1, 1, 1,1},
				};
				glm::vec3 world[8];
				for (int i = 0; i < 8; i++) { glm::vec4 w = invVP * corners[i]; world[i] = glm::vec3(w) / w.w; }
				float vpW = m_ViewportBounds[1].x - m_ViewportBounds[0].x;
				float vpH = m_ViewportBounds[1].y - m_ViewportBounds[0].y;
				glm::mat4 vp = proj * view;
				ImVec2 screen[8];
				for (int i = 0; i < 8; i++) {
					glm::vec4 c = vp * glm::vec4(world[i], 1.0f);
					if (c.w != 0) c /= c.w;
					screen[i] = ImVec2((c.x*0.5f+0.5f)*vpW + m_ViewportBounds[0].x, ((1.0f-c.y)*0.5f)*vpH + m_ViewportBounds[0].y);
				}
				auto* dl = ImGui::GetWindowDrawList();
				ImU32 col = IM_COL32(255, 255, 100, 80);
				for (int i = 0; i < 4; i++) {
					dl->AddLine(screen[i], screen[(i+1)%4], col);
					dl->AddLine(screen[i+4], screen[(i+1)%4+4], col);
					dl->AddLine(screen[i], screen[i+4], col);
				}
			}

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

			// --- 鼠标拾取（左键点击实体选择） ---
			static constexpr uint32_t kPickAttachment = 1;
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
			    && !ImGuizmo::IsOver()
			    && m_ViewportHovered)
			{
				auto [mx, my] = ImGui::GetMousePos();
				auto& spec = m_Framebuffer->GetSpecification();
				int fbX = (int)((mx - m_ViewportBounds[0].x) / (m_ViewportBounds[1].x - m_ViewportBounds[0].x) * spec.Width);
				int fbY = (int)((1.0f - (my - m_ViewportBounds[0].y) / (m_ViewportBounds[1].y - m_ViewportBounds[0].y)) * spec.Height);

				if (fbX >= 0 && fbY >= 0)
				{
					int id = m_Framebuffer->ReadPixel(kPickAttachment, fbX, fbY);
					if (id >= 0)
						m_HierarchyPanel.SetSelectedEntity(m_ActiveScene->GetEntityByID((uint32_t)id));
					else
						m_HierarchyPanel.SetSelectedEntity({});
				}
			}
		} // SceneState::Edit

		ImGui::End();
		ImGui::PopStyleVar();
		ImGui::End();
	}

	void EditorLayer::OnEvent(Event& event) {
		GL_TRACE("{0}", event.ToString());

		if (event.IsInCategory(EventCategoryKeyboard)) {
			if (ImGui::GetIO().WantCaptureKeyboard) return;
		}

		if (event.IsInCategory(EventCategoryMouse)) {
			if (!m_ViewportHovered) return;
		}

		if (m_SceneState == SceneState::Edit)
			m_EditorCamera.OnEvent(event);
	}

}
