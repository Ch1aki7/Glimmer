#include "EditorLayer.h"
#include "Glimmer/Scene/SceneSerializer.h"
#include "Glimmer/Utils/FileDialog.h"
#include "Glimmer/Core/Input.h"
#include "Glimmer/Renderer/RenderPass.h"
#include <glm/gtc/type_ptr.hpp>
#include <ImGuizmo.h>

#include <array>
#include <algorithm>
#include <cstdlib>
#include <vector>
namespace gl {
	namespace {
		bool ShouldAutorunPBRLab()
		{
#ifdef GL_PLATFORM_WINDOWS
			char* value = nullptr;
			size_t length = 0;
			const bool present = _dupenv_s(&value, &length,
				"GLIMMER_PBR_LAB_AUTORUN") == 0 && value != nullptr;
			std::free(value);
			return present;
#else
			return std::getenv("GLIMMER_PBR_LAB_AUTORUN") != nullptr;
#endif
		}
	}

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"),
		  m_ShaderPanel(&m_ShaderLib),
		  m_EditorCamera(45.0f, 1280.0f / 720.0f) {
	}

	void EditorLayer::SetEditorScene(const Ref<Scene>& scene)
	{
		GL_CORE_ASSERT(scene, "Editor scene cannot be null.");
		if (m_DebugPanel.IsTemporarySceneActive())
			m_DebugPanel.ExitTemporaryTools();
		if (m_SceneState == SceneState::Play)
			OnSceneStop();

		m_CommandHistory.Clear();
		m_EditorScene = scene;
		m_ActiveScene = m_EditorScene;
		m_HierarchyPanel.SetContext(m_ActiveScene);
		m_HierarchyPanel.SetSelectionContext(&m_SelectionContext);
		m_HierarchyPanel.SetCommandHistory(&m_CommandHistory);
		m_InspectorPanel.SetContext(m_ActiveScene);
		m_InspectorPanel.SetSelectionContext(&m_SelectionContext);
		m_InspectorPanel.SetCommandHistory(&m_CommandHistory);
		m_HierarchyPanel.SetSelectedEntity({});
	}

	bool EditorLayer::ActivateTemporaryDebugScene(const Ref<Scene>& scene)
	{
		if (!scene || m_SceneState != SceneState::Edit || !m_EditorScene)
			return false;

		m_ActiveScene = scene;
		m_HierarchyPanel.SetContext(m_ActiveScene);
		m_InspectorPanel.SetContext(m_ActiveScene);
		m_HierarchyPanel.SetCommandHistory(nullptr);
		m_InspectorPanel.SetCommandHistory(nullptr);
		m_HierarchyPanel.SetSelectedEntity({});
		GL_CORE_INFO("Temporary debug scene activated.");
		return true;
	}

	void EditorLayer::ExitTemporaryDebugScene()
	{
		if (!m_EditorScene || m_SceneState != SceneState::Edit)
			return;

		m_ActiveScene = m_EditorScene;
		m_HierarchyPanel.SetContext(m_ActiveScene);
		m_InspectorPanel.SetContext(m_ActiveScene);
		m_HierarchyPanel.SetCommandHistory(&m_CommandHistory);
		m_InspectorPanel.SetCommandHistory(&m_CommandHistory);
		m_HierarchyPanel.SetSelectedEntity({});
		GL_CORE_INFO("Temporary debug scene exited; editor scene restored.");
	}

	void EditorLayer::OnScenePlay()
	{
		if (m_SceneState != SceneState::Edit || !m_EditorScene)
			return;
		if (m_DebugPanel.IsTemporarySceneActive())
			m_DebugPanel.ExitTemporaryTools();

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
		m_InspectorPanel.SetContext(m_ActiveScene);
		m_HierarchyPanel.SetCommandHistory(nullptr);
		m_InspectorPanel.SetCommandHistory(nullptr);
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
		m_InspectorPanel.SetContext(m_ActiveScene);
		m_HierarchyPanel.SetCommandHistory(&m_CommandHistory);
		m_InspectorPanel.SetCommandHistory(&m_CommandHistory);
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
		const AssetHandle defaultSkyboxHandle =
			AssetManager::ImportAsset("assets/skyboxes/desert-evening.glsky");
		const AssetHandle defaultInstancingModelHandle =
			AssetManager::ImportAsset("assets/models/geos/Cube.obj");
		const AssetHandle defaultInstancingMaterialHandle =
			AssetManager::ImportAsset("assets/materials/DefaultPBR.glmat");
		const AssetHandle defaultPbrSphereHandle =
			AssetManager::ImportAsset("assets/models/geos/UV Sphere.obj");
		const AssetHandle defaultNormalTextureHandle =
			AssetManager::ImportAsset("assets/textures/NoiseTex.png");
		const AssetHandle defaultAOTextureHandle =
			AssetManager::ImportAsset("assets/textures/heightmap-example.png");
		const AssetHandle defaultEmissiveTextureHandle =
			AssetManager::ImportAsset("assets/textures/Henry.jpg");
		AssetManager::SetTextureMetadata(defaultNormalTextureHandle,
			TextureColorSpace::Linear, TextureSemantic::Normal);
		m_SkyboxShader = m_ShaderLib.Load(
			"Skybox", "assets/shaders/Skybox.glsl");
		const AssetHandle terrainShaderHandle =
			AssetManager::ImportAsset("assets/shaders/Terrain.glsl");
		const AssetHandle terrainGenerationShaderHandle =
			AssetManager::ImportAsset("assets/shaders/Terrain/GenerateFBM.comp");
		const AssetHandle terrainErosionShaderHandle =
			AssetManager::ImportAsset("assets/shaders/Terrain/ThermalErosion.comp");
		const AssetHandle terrainDerivationShaderHandle =
			AssetManager::ImportAsset("assets/shaders/Terrain/DeriveTerrainMaps.comp");

		FramebufferSpecification sceneFramebufferSpec;
		sceneFramebufferSpec.Width = 1280;
		sceneFramebufferSpec.Height = 720;
		sceneFramebufferSpec.Attachments = {
			{ FramebufferTextureFormat::RGBA16F },
			{ FramebufferTextureFormat::RED_INTEGER }  // 鼠标拾取
		};
		m_Framebuffer = Framebuffer::Create(sceneFramebufferSpec);

		FramebufferSpecification displayFramebufferSpec;
		displayFramebufferSpec.Width = 1280;
		displayFramebufferSpec.Height = 720;
		displayFramebufferSpec.Attachments = { { FramebufferTextureFormat::RGBA8 } };
		m_DisplayFramebuffer = Framebuffer::Create(displayFramebufferSpec);

		m_ShaderLib.Load("assets/shaders/ToneMapping.glsl");
		m_ShaderLib.Load("assets/shaders/Overlay.glsl");
		m_ShaderLib.Load("Phong", "assets/shaders/Phong.glsl");
		m_ShaderLib.Load("Toon", "assets/shaders/Toon.glsl");
		m_ShaderLib.Load("Blinn-Phong", "assets/shaders/BlinnPhong.glsl");
		m_ShaderLib.Load("Hologram", "assets/shaders/Hologram.glsl");

		// --- 场景 ---
		SetEditorScene(CreateRef<Scene>());

		auto sunEntity = m_ActiveScene->CreateEntity("Sun");
		sunEntity.AddComponent<DirectionalLightComponent>();
		sunEntity.GetComponent<TransformComponent>().Rotation = { -50.0f, 30.0f, 0.0f };

		auto pointLightEntity = m_ActiveScene->CreateEntity("Point Light");
		auto& pointLight = pointLightEntity.AddComponent<PointLightComponent>();
		pointLight.Intensity = 80.0f;
		pointLight.Range = 40.0f;
		pointLightEntity.GetComponent<TransformComponent>().Translation = { 0.0f, 12.0f, 0.0f };

		auto skyLightEntity = m_ActiveScene->CreateEntity("Sky Light");
		skyLightEntity.AddComponent<SkyLightComponent>(defaultSkyboxHandle);

		auto terrainEntity = m_ActiveScene->CreateEntity("Terrain");
		auto& terrain = terrainEntity.AddComponent<TerrainComponent>();
		ApplyTerrainPreset(terrain.Specification, TerrainPreset::Alpine);
		terrain.Specification.RenderShaderHandle = terrainShaderHandle;
		terrain.Specification.GenerationShaderHandle = terrainGenerationShaderHandle;
		terrain.Specification.ErosionShaderHandle = terrainErosionShaderHandle;
		terrain.Specification.DerivationShaderHandle = terrainDerivationShaderHandle;
		// Keep the startup terrain texture-free. Assigning a TerrainMaterial later
		// opts into the full four-layer Triplanar texture path.
		terrain.Specification.TerrainMaterialHandle = AssetHandle(0);


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
		m_ContentBrowser.OnAssetSelected = [this](AssetHandle handle) {
			m_SelectionContext.SelectAsset(handle);
			m_HierarchyPanel.SetSelectedEntity({});
			m_SelectionContext.SelectAsset(handle);
		};

		m_DebugPanel.SetDefaultAssets(
			defaultInstancingModelHandle,
			defaultInstancingMaterialHandle,
			defaultSkyboxHandle,
			defaultPbrSphereHandle,
			defaultNormalTextureHandle,
			defaultAOTextureHandle,
			defaultEmissiveTextureHandle);
		m_DebugPanel.SetTemporarySceneCallbacks(
			[this](const Ref<Scene>& scene) {
				return ActivateTemporaryDebugScene(scene);
			},
			[this]() {
				ExitTemporaryDebugScene();
			},
			[this](Entity entity) {
				m_HierarchyPanel.SetSelectedEntity(entity);
			});
		if (ShouldAutorunPBRLab())
			m_DebugPanel.GeneratePBRMaterialLabForValidation();


	}
	void EditorLayer::OnDetach() {
		GL_PROFILE_FUNCTION();
		if (m_DebugPanel.IsTemporarySceneActive())
			m_DebugPanel.ExitTemporaryTools();
		if (m_SceneState == SceneState::Play)
			OnSceneStop();
		AssetManager::Shutdown();
	}

	void EditorLayer::OnUpdate(Timestep ts) {
		GL_PROFILE_FUNCTION();
		m_ShaderPanel.OnUpdate();

		// --- 编辑器相机（仅编辑模式） ---
		m_EditorCamera.SetInputEnabled(m_ViewportHovered);
		if (m_SceneState == SceneState::Edit)
			m_EditorCamera.OnUpdate(ts);

		Renderer2D::ResetStats();

		if (m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f)
		{
			const uint32_t viewportWidth = static_cast<uint32_t>(m_ViewportSize.x);
			const uint32_t viewportHeight = static_cast<uint32_t>(m_ViewportSize.y);
			const auto& framebufferSpecification = m_Framebuffer->GetSpecification();
			if (framebufferSpecification.Width != viewportWidth
				|| framebufferSpecification.Height != viewportHeight)
			{
				m_Framebuffer->Resize(viewportWidth, viewportHeight);
				m_DisplayFramebuffer->Resize(viewportWidth, viewportHeight);
			}

			m_EditorCamera.SetViewportSize(
				static_cast<float>(viewportWidth),
				static_cast<float>(viewportHeight));
			if (m_ActiveScene)
				m_ActiveScene->OnViewportResize(viewportWidth, viewportHeight);
		}

		RenderPassSpecification scenePass;
		scenePass.Target = m_Framebuffer;
		scenePass.ClearColorValue = { 0.1f, 0.1f, 0.1f, 1 };
		RenderPass::Begin(scenePass);
		m_Framebuffer->ClearAttachment(1, -1);

		{
			GL_PROFILE_SCOPE("Scene Draw");
			glm::mat4 skyboxView{ 1.0f };
			glm::mat4 skyboxProjection{ 1.0f };
			bool hasSkyboxCamera = false;

			if (m_SceneState == SceneState::Edit)
			{
				skyboxView = m_EditorCamera.GetViewMatrix();
				skyboxProjection = m_EditorCamera.GetProjectionMatrix();
				hasSkyboxCamera = true;
				m_ActiveScene->OnUpdateEditor(
					ts, skyboxView, skyboxProjection,
					m_EditorCamera.GetPosition(),
					m_EditorCamera.GetNearClip(), m_EditorCamera.GetFarClip(), true);
			}
			else
			{
				m_ActiveScene->OnUpdateRuntime(ts, true);
				Entity cameraEntity = m_ActiveScene->GetPrimaryCameraEntity();
				if (cameraEntity
					&& cameraEntity.HasComponent<CameraComponent>()
					&& cameraEntity.HasComponent<TransformComponent>())
				{
					skyboxProjection = cameraEntity
						.GetComponent<CameraComponent>().Camera.GetProjection();
					skyboxView = glm::inverse(cameraEntity
						.GetComponent<TransformComponent>().GetTransform());
					hasSkyboxCamera = true;
				}
			}

			Entity skyLightEntity = m_ActiveScene->GetSkyLightEntity();
			if (hasSkyboxCamera && skyLightEntity)
			{
				const auto& skyLight =
					skyLightEntity.GetComponent<SkyLightComponent>();
				if (Ref<Cubemap> cubemap =
					AssetManager::GetCubemap(skyLight.CubemapHandle))
				{
					SkyboxRenderer::Draw(
						cubemap->GetTexture(),
						m_SkyboxShader,
						skyboxView,
						skyboxProjection,
						skyLight.Intensity);
				}
			}
			m_ActiveScene->FlushSpritePass();
			Renderer3D::EndScene();
		}

		RenderPass::End();

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

		RenderPassSpecification toneMappingPass;
		toneMappingPass.Target = m_DisplayFramebuffer;
		RenderPass::Begin(toneMappingPass);
		auto toneMappingShader = m_ShaderLib.Get("ToneMapping");
		toneMappingShader->Bind();
		toneMappingShader->UploadUniformFloat("u_Exposure", m_Exposure);
		toneMappingShader->UploadUniformInt("u_ApplyGrayscale", m_GrayscaleEnabled ? 1 : 0);
		Renderer2D::DrawPostProcess(
			toneMappingShader, m_Framebuffer->GetColorAttachmentRendererID());
		RenderPass::End();
		m_FinalSceneTexture = m_DisplayFramebuffer->GetColorAttachmentRendererID();
	}

	void EditorLayer::OnImGuiRender() {
		GL_PROFILE_FUNCTION();

		// --- 全局快捷键 ---
		auto& io = ImGui::GetIO();
		if (m_SceneState == SceneState::Edit
			&& !m_DebugPanel.IsTemporarySceneActive()
			&& ImGui::IsKeyChordPressed(ImGuiKey_Z | ImGuiMod_Ctrl))
			m_CommandHistory.Undo();
		if (m_SceneState == SceneState::Edit
			&& !m_DebugPanel.IsTemporarySceneActive()
			&& (ImGui::IsKeyChordPressed(ImGuiKey_Y | ImGuiMod_Ctrl)
				|| ImGui::IsKeyChordPressed(ImGuiKey_Z | ImGuiMod_Ctrl | ImGuiMod_Shift)))
			m_CommandHistory.Redo();

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
			if (m_DebugPanel.IsTemporarySceneActive())
				GL_CORE_WARN("Exit the temporary Debug Lab before saving the editor scene.");
			else
			{
				std::string path = FileDialog::SaveFile("Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
				if (!path.empty()) {
					SceneSerializer serializer(m_EditorScene);
					serializer.Serialize(path);
				}
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
				ImGui::BeginDisabled(m_DebugPanel.IsTemporarySceneActive());
				if (ImGui::MenuItem("Save As...", "Ctrl+S"))
				{
					std::string path = FileDialog::SaveFile("Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
					if (!path.empty()) {
						SceneSerializer serializer(m_EditorScene);
						serializer.Serialize(path);
					}
				}
				ImGui::EndDisabled();
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
			if (ImGui::BeginMenu("Window"))
			{
				if (ImGui::MenuItem("Debug", nullptr, m_DebugPanel.IsOpen()))
					m_DebugPanel.SetOpen(!m_DebugPanel.IsOpen());
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
		if (m_DebugPanel.IsTemporarySceneActive())
		{
			ImGui::Begin("Scene Hierarchy");
			ImGui::TextDisabled("Instancing Lab is active.");
			ImGui::TextWrapped(
				"Hierarchy enumeration is disabled so thousands of debug entities do not distort the render stress test.");
			ImGui::End();
		}
		else
			m_HierarchyPanel.OnImGuiRender();
		m_InspectorPanel.OnImGuiRender();

		// --- Content Browser ---
		m_ContentBrowser.OnImGuiRender();
		m_ShaderPanel.OnImGuiRender();

		// Stats
		ImGui::Begin("Stats");
		auto stats = Renderer2D::GetStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %d", stats.DrawCalls);
		ImGui::Text("Quads: %d", stats.QuadCount);
		ImGui::Separator();
		const auto stats3D = Renderer3D::GetStats();
		ImGui::Text("Renderer3D Queues:");
		ImGui::Text("Models / Items: %u / %u",
			stats3D.SubmittedModels, stats3D.SubmittedItems);
		ImGui::Text("Opaque / Mask / Transparent: %u / %u / %u",
			stats3D.OpaqueItems, stats3D.MaskItems, stats3D.TransparentItems);
		ImGui::Text("Skipped Models: %u", stats3D.SkippedModels);
		ImGui::Text("Draw Calls: %u", stats3D.DrawCalls);
		ImGui::Text("Instanced / Individual Draws: %u / %u",
			stats3D.InstancedDrawCalls, stats3D.IndividualDrawCalls);
		ImGui::Text("Transparent Draws: %u", stats3D.TransparentDrawCalls);
		ImGui::Text("Batches / Instances: %u / %u",
			stats3D.BatchCount, stats3D.InstanceCount);
		ImGui::Text("Saved Draws: %u", stats3D.GetSavedDrawCalls());
		ImGui::Text("Material Cache Hit / Miss: %u / %u",
			stats3D.MaterialCacheHits, stats3D.MaterialCacheMisses);
		ImGui::Text("Shader Binds: %u (saved %u)",
			stats3D.ShaderBinds, stats3D.GetSavedShaderBinds());
		ImGui::Text("Texture Binds: %u (saved %u)",
			stats3D.TextureBinds, stats3D.GetSavedTextureBinds());
		ImGui::End();
		m_DebugPanel.OnImGuiRender(stats3D);

		// Settings
		ImGui::Begin("Settings");
		ImGui::SeparatorText("HDR Output");
		ImGui::DragFloat("Exposure", &m_Exposure, 0.05f, 0.01f, 10.0f);
		ImGui::Checkbox("Grayscale", &m_GrayscaleEnabled);
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
		m_ViewportSize = {
			static_cast<float>(static_cast<uint32_t>(std::max(viewportPanelSize.x, 0.0f))),
			static_cast<float>(static_cast<uint32_t>(std::max(viewportPanelSize.y, 0.0f)))
		};

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
				}				else if (ext == ".glterrainmat")
				{
					const AssetHandle handle = AssetManager::ImportAsset(path);
					if (AssetManager::GetMetadata(handle).Type == AssetType::TerrainMaterial)
					{
						Entity terrainEntity = m_HierarchyPanel.GetSelectedEntity();
						if (!terrainEntity || !terrainEntity.HasComponent<TerrainComponent>())
						{
							terrainEntity = m_ActiveScene->CreateEntity("Terrain");
							auto& terrain = terrainEntity.AddComponent<TerrainComponent>();
							terrain.Specification.RenderShaderHandle =
								AssetManager::ImportAsset("assets/shaders/Terrain.glsl");
							terrain.Specification.GenerationShaderHandle =
								AssetManager::ImportAsset("assets/shaders/Terrain/GenerateFBM.comp");
							terrain.Specification.ErosionShaderHandle =
								AssetManager::ImportAsset("assets/shaders/Terrain/ThermalErosion.comp");
							terrain.Specification.DerivationShaderHandle =
								AssetManager::ImportAsset("assets/shaders/Terrain/DeriveTerrainMaps.comp");
						}
						terrainEntity.GetComponent<TerrainComponent>()
							.Specification.TerrainMaterialHandle = handle;
						m_HierarchyPanel.SetSelectedEntity(terrainEntity);
					}
				}				else if (ext == ".glsky")
				{
					AssetHandle handle = AssetManager::ImportAsset(path);
					if (AssetManager::GetMetadata(handle).Type
						== AssetType::Cubemap)
					{
						Entity skyLightEntity =
							m_HierarchyPanel.GetSelectedEntity();
						if (!skyLightEntity)
							skyLightEntity =
								m_ActiveScene->CreateEntity("Sky Light");
						if (!skyLightEntity.HasComponent<SkyLightComponent>())
							skyLightEntity.AddComponent<SkyLightComponent>(handle);
						else
							skyLightEntity.GetComponent<SkyLightComponent>()
								.CubemapHandle = handle;
						m_HierarchyPanel.SetSelectedEntity(skyLightEntity);
					}
				}				else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg"
					|| ext == ".tga" || ext == ".bmp")
				{
					const AssetHandle heightMapHandle = AssetManager::ImportAsset(path);
					if (AssetManager::GetMetadata(heightMapHandle).Type == AssetType::Texture2D)
					{
						Entity terrainEntity = m_ActiveScene->CreateEntity("Terrain");
						auto& terrain = terrainEntity.AddComponent<TerrainComponent>();
						terrain.Specification.Procedural = false;
						terrain.Specification.HeightMapHandle = heightMapHandle;
						terrain.Specification.RenderShaderHandle =
							AssetManager::ImportAsset("assets/shaders/Terrain.glsl");
						terrain.Specification.GenerationShaderHandle =
							AssetManager::ImportAsset("assets/shaders/Terrain/GenerateFBM.comp");
						terrain.Specification.ErosionShaderHandle =
							AssetManager::ImportAsset("assets/shaders/Terrain/ThermalErosion.comp");
						terrain.Specification.DerivationShaderHandle =
							AssetManager::ImportAsset("assets/shaders/Terrain/DeriveTerrainMaps.comp");
						terrain.Specification.TerrainMaterialHandle =
							AssetManager::ImportAsset("assets/materials/DefaultTerrain.glterrainmat");
						m_HierarchyPanel.SetSelectedEntity(terrainEntity);
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
