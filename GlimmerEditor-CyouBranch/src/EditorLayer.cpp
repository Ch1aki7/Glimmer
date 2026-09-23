#include "EditorLayer.h"
#include "Debug/TerrainValidationScene.h"
#include "Editor/EditorScenePreferences.h"
#include "Glimmer/Scene/SceneSerializer.h"
#include "Glimmer/Utils/FileDialog.h"
#include "Glimmer/Core/Input.h"
#include "Glimmer/Renderer/RenderPass.h"
#include <glm/gtc/type_ptr.hpp>
#include <ImGuizmo.h>

#include <array>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <limits>
#include <vector>
namespace gl {
	namespace {
		std::string GetEnvironmentValue(const char* name)
		{
#ifdef GL_PLATFORM_WINDOWS
			char* value = nullptr;
			size_t length = 0;
			if (_dupenv_s(&value, &length, name) != 0 || !value)
				return {};
			std::string result(value);
			std::free(value);
			return result;
#else
			const char* value = std::getenv(name);
			return value ? value : "";
#endif
		}

		bool HasEnvironmentVariable(const char* name)
		{
#ifdef GL_PLATFORM_WINDOWS
			char* value = nullptr;
			size_t length = 0;
			const bool present = _dupenv_s(&value, &length, name) == 0
				&& value != nullptr;
			std::free(value);
			return present;
#else
			return std::getenv(name) != nullptr;
#endif
		}

		bool ShouldAutorunPBRLab()
		{
			return HasEnvironmentVariable("GLIMMER_PBR_LAB_AUTORUN");
		}

		bool ShouldAutorunToonLab()
		{
			return HasEnvironmentVariable("GLIMMER_TOON_LAB_AUTORUN");
		}

		bool ShouldValidatePostProcess()
		{
			return HasEnvironmentVariable("GLIMMER_POST_PROCESS_VALIDATE");
		}

		bool ShouldAutorunShadowBenchmark()
		{
			return HasEnvironmentVariable("GLIMMER_SHADOW_BENCHMARK_AUTORUN");
		}

		bool ShouldAutorunTerrainSamplingBenchmark()
		{
			return HasEnvironmentVariable(
				"GLIMMER_TERRAIN_SAMPLING_BENCHMARK_AUTORUN");
		}

		int GetTerrainSamplingVisualMode()
		{
#ifdef GL_PLATFORM_WINDOWS
			char* value = nullptr;
			size_t length = 0;
			if (_dupenv_s(&value, &length,
				"GLIMMER_TERRAIN_SAMPLING_VISUAL_MODE") != 0 || !value)
				return -1;
			const int mode = std::atoi(value);
			std::free(value);
			return mode;
#else
			const char* value =
				std::getenv("GLIMMER_TERRAIN_SAMPLING_VISUAL_MODE");
			return value ? std::atoi(value) : -1;
#endif
		}

		uint32_t GetTerrainSynthesisVersion()
		{
			const std::string value = GetEnvironmentValue(
				"GLIMMER_TERRAIN_SYNTHESIS_VERSION");
			return value.empty() ? 2u
				: std::clamp(static_cast<uint32_t>(std::atoi(value.c_str())), 1u, 2u);
		}

		bool WriteRgbaBmp(const std::filesystem::path& path,
			const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height)
		{
			if (width == 0 || height == 0
				|| rgba.size() != static_cast<size_t>(width) * height * 4u)
				return false;
			std::error_code error;
			if (!path.parent_path().empty())
				std::filesystem::create_directories(path.parent_path(), error);
			if (error)
				return false;

			const uint32_t rowBytes = width * 3u;
			const uint32_t rowStride = (rowBytes + 3u) & ~3u;
			const uint32_t pixelBytes = rowStride * height;
			const uint32_t fileBytes = 54u + pixelBytes;
			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			if (!output)
				return false;
			auto write16 = [&output](uint16_t value) {
				output.put(static_cast<char>(value & 0xffu));
				output.put(static_cast<char>((value >> 8u) & 0xffu));
			};
			auto write32 = [&output](uint32_t value) {
				for (uint32_t shift = 0; shift < 32u; shift += 8u)
					output.put(static_cast<char>((value >> shift) & 0xffu));
			};
			output.put('B'); output.put('M');
			write32(fileBytes); write16(0); write16(0); write32(54);
			write32(40); write32(width); write32(height); write16(1); write16(24);
			write32(0); write32(pixelBytes); write32(2835); write32(2835);
			write32(0); write32(0);
			const std::array<char, 3> padding{};
			for (uint32_t y = 0; y < height; ++y)
			{
				for (uint32_t x = 0; x < width; ++x)
				{
					const size_t offset = (static_cast<size_t>(y) * width + x) * 4u;
					output.put(static_cast<char>(rgba[offset + 2]));
					output.put(static_cast<char>(rgba[offset + 1]));
					output.put(static_cast<char>(rgba[offset]));
				}
				output.write(padding.data(), rowStride - rowBytes);
			}
			return output.good();
		}

		bool ShouldAutorunShadowVisualValidation()
		{
			return HasEnvironmentVariable("GLIMMER_SHADOW_VISUAL_AUTORUN");
		}

		bool ShouldVisualizeShadowCascades()
		{
			return HasEnvironmentVariable("GLIMMER_SHADOW_VISUALIZE_CASCADES");
		}

		bool ShouldVisualizeTerrainLODs()
		{
			return HasEnvironmentVariable("GLIMMER_TERRAIN_LOD_VISUALIZE");
		}

		bool ShouldVisualizeDistanceFog()
		{
			return HasEnvironmentVariable("GLIMMER_DISTANCE_FOG_VISUALIZE");
		}

		bool ShouldUseShadowCasterCloseup()
		{
			return HasEnvironmentVariable("GLIMMER_SHADOW_VISUAL_CLOSEUP");
		}

		void EncapsulateTransformedBounds(
			const glm::vec3& boundsMin,
			const glm::vec3& boundsMax,
			const glm::mat4& transform,
			glm::vec3& worldMin,
			glm::vec3& worldMax)
		{
			for (uint32_t x = 0; x < 2; ++x)
			{
				for (uint32_t y = 0; y < 2; ++y)
				{
					for (uint32_t z = 0; z < 2; ++z)
					{
						const glm::vec3 corner{
							x ? boundsMax.x : boundsMin.x,
							y ? boundsMax.y : boundsMin.y,
							z ? boundsMax.z : boundsMin.z };
						const glm::vec3 worldCorner =
							glm::vec3(transform * glm::vec4(corner, 1.0f));
						worldMin = glm::min(worldMin, worldCorner);
						worldMax = glm::max(worldMax, worldCorner);
					}
				}
			}
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
		if (m_PostProcessRenderer.IsInitialized())
			m_PostProcessRenderer.ResetHistory();
		m_GizmoTransformEdit.Reset();
		m_GizmoEditScene.reset();
		m_GizmoEditEntity = UUID(0);
		m_EditorScene = scene;
		m_ActiveScene = m_EditorScene;
		m_HierarchyPanel.SetContext(m_ActiveScene);
		m_HierarchyPanel.SetSelectionContext(&m_SelectionContext);
		m_HierarchyPanel.SetCommandHistory(&m_CommandHistory);
		m_InspectorPanel.SetContext(m_ActiveScene);
		m_InspectorPanel.SetSelectionContext(&m_SelectionContext);
		m_InspectorPanel.SetCommandHistory(&m_CommandHistory);
		m_HierarchyPanel.SetSelectedEntity({});
		CaptureSavedSceneState();
	}

	void EditorLayer::CaptureSavedSceneState()
	{
		m_SavedSceneSnapshot.clear();
		if (!m_EditorScene
			|| !SceneSerializer(m_EditorScene).SerializeToString(
				m_SavedSceneSnapshot))
		{
			m_SceneDirty = true;
			UpdateWindowTitle();
			return;
		}
		m_SceneDirty = false;
		m_NextDirtyCheckTime = Application::GetTime() + 0.5f;
		UpdateWindowTitle();
	}

	bool EditorLayer::RefreshSceneDirtyState()
	{
		std::string currentSnapshot;
		if (!m_EditorScene
			|| !SceneSerializer(m_EditorScene).SerializeToString(currentSnapshot))
		{
			m_SceneDirty = true;
			UpdateWindowTitle();
			return true;
		}
		m_SceneDirty = currentSnapshot != m_SavedSceneSnapshot;
		UpdateWindowTitle();
		return m_SceneDirty;
	}

	void EditorLayer::UpdateWindowTitle()
	{
		const std::string sceneName = m_EditorScenePath.empty()
			? "Untitled" : m_EditorScenePath.filename().string();
		const std::string title = sceneName + (m_SceneDirty ? "*" : "")
			+ " - Glimmer Editor - Cyou Branch";
		if (title == m_LastWindowTitle)
			return;
		Application::Get().GetWindow().SetTitle(title);
		m_LastWindowTitle = title;
	}

	void EditorLayer::FinishGizmoTransformEdit()
	{
		if (!m_GizmoTransformEdit.IsActive())
			return;

		const TransformComponent before = m_GizmoTransformEdit.GetBefore();
		const Ref<Scene> scene = m_GizmoEditScene;
		const UUID uuid = m_GizmoEditEntity;
		const int gizmoType = m_GizmoEditType;
		m_GizmoTransformEdit.Reset();
		m_GizmoEditScene.reset();
		m_GizmoEditEntity = UUID(0);

		Entity target = scene ? scene->FindEntityByUUID(uuid) : Entity{};
		if (scene != m_EditorScene
			|| !target || !target.HasComponent<TransformComponent>())
			return;

		const TransformComponent after =
			target.GetComponent<TransformComponent>();
		const bool unchanged = glm::all(glm::equal(
			before.Translation, after.Translation))
			&& glm::all(glm::equal(before.Rotation, after.Rotation))
			&& glm::all(glm::equal(before.Scale, after.Scale));
		if (unchanged)
			return;

		auto apply = [scene, uuid](const TransformComponent& value) {
			Entity entity = scene ? scene->FindEntityByUUID(uuid) : Entity{};
			if (!entity || !entity.HasComponent<TransformComponent>())
				return false;
			entity.GetComponent<TransformComponent>() = value;
			return true;
		};
		const char* commandName = gizmoType == 1 ? "Rotate Entity"
			: gizmoType == 2 ? "Scale Entity" : "Move Entity";
		m_CommandHistory.PushExecuted(
			std::make_unique<ValueEditorCommand<TransformComponent>>(
				commandName, before, after, std::move(apply)));
	}

	void EditorLayer::RememberCurrentScene() const
	{
		if (m_UsesTerrainValidationScene)
			return;
		if (!EditorScenePreferences::StoreLastScene(
			std::filesystem::absolute("assets").lexically_normal(),
			m_EditorScenePath.empty()
				? std::optional<std::filesystem::path>{}
				: std::optional<std::filesystem::path>{ m_EditorScenePath }))
			GL_CORE_WARN("Could not persist the current editor scene path.");
	}

	void EditorLayer::PersistEditorCameraState() const
	{
		if (m_EditorScenePath.empty()
			|| m_DebugPanel.IsTemporarySceneActive())
			return;
		if (!EditorScenePreferences::StoreCameraState(
			std::filesystem::absolute("assets").lexically_normal(),
			m_EditorScenePath, m_EditorCamera.GetState()))
			GL_CORE_WARN("Could not persist the editor camera state.");
	}

	void EditorLayer::RestoreEditorCameraState()
	{
		if (!m_EditorScenePath.empty())
		{
			const auto state = EditorScenePreferences::LoadCameraState(
				std::filesystem::absolute("assets").lexically_normal(),
				m_EditorScenePath);
			if (state && m_EditorCamera.SetState(*state))
			{
				GL_CORE_INFO("Restored editor camera for scene: {0}",
					m_EditorScenePath.string());
				return;
			}
		}
		m_EditorCamera.SetState(EditorCameraState{});
	}

	void EditorLayer::NewScene()
	{
		PersistEditorCameraState();
		SetEditorScene(CreateRef<Scene>());
		m_EditorScenePath.clear();
		m_EditorCamera.SetState(EditorCameraState{});
		UpdateWindowTitle();
		RememberCurrentScene();
		GL_CORE_INFO("Created an empty editor scene.");
	}

	bool EditorLayer::OpenScene(const std::filesystem::path& path)
	{
		if (path.empty() || path.extension() != ".glimmer")
			return false;

		auto newScene = CreateRef<Scene>();
		SceneSerializer serializer(newScene);
		if (!serializer.Deserialize(path.string()))
		{
			GL_CORE_WARN("Could not open scene: {0}", path.string());
			return false;
		}

		std::error_code error;
		std::filesystem::path openedPath =
			std::filesystem::weakly_canonical(path, error);
		if (error)
			openedPath = std::filesystem::absolute(path).lexically_normal();
		PersistEditorCameraState();
		SetEditorScene(newScene);
		m_EditorScenePath = std::move(openedPath);
		UpdateWindowTitle();
		RestoreEditorCameraState();
		RememberCurrentScene();
		GL_CORE_INFO("Loaded scene: {0}", m_EditorScenePath.string());
		return true;
	}

	bool EditorLayer::SaveScene()
	{
		m_SceneSaveError.clear();
		if (m_DebugPanel.IsTemporarySceneActive())
		{
			GL_CORE_WARN("Exit the temporary Debug Lab before saving the editor scene.");
			m_SceneSaveError =
				"Exit the temporary Debug Lab before saving the editor scene.";
			return false;
		}
		if (m_EditorScenePath.empty())
			return SaveSceneAs();

		if (!SceneSerializer(m_EditorScene).Serialize(m_EditorScenePath.string()))
		{
			m_SceneSaveError = "Could not save the scene. The previous file was left unchanged.";
			return false;
		}
		m_SceneSaveError.clear();
		CaptureSavedSceneState();
		PersistEditorCameraState();
		RememberCurrentScene();
		GL_CORE_INFO("Saved scene: {0}", m_EditorScenePath.string());
		return true;
	}

	bool EditorLayer::SaveSceneAs()
	{
		m_SceneSaveError.clear();
		if (m_DebugPanel.IsTemporarySceneActive())
		{
			GL_CORE_WARN("Exit the temporary Debug Lab before saving the editor scene.");
			m_SceneSaveError =
				"Exit the temporary Debug Lab before saving the editor scene.";
			return false;
		}
		const std::string selectedPath = FileDialog::SaveFile(
			"Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
		if (selectedPath.empty())
			return false;
		std::filesystem::path path(selectedPath);
		if (path.extension() != ".glimmer")
			path += ".glimmer";
		if (!SceneSerializer(m_EditorScene).Serialize(path.string()))
		{
			m_SceneSaveError = "Could not save the scene. The previous file was left unchanged.";
			return false;
		}
		m_SceneSaveError.clear();

		PersistEditorCameraState();
		std::error_code error;
		m_EditorScenePath = std::filesystem::weakly_canonical(path, error);
		if (error)
			m_EditorScenePath = std::filesystem::absolute(path).lexically_normal();
		CaptureSavedSceneState();
		PersistEditorCameraState();
		RememberCurrentScene();
		GL_CORE_INFO("Saved scene: {0}", m_EditorScenePath.string());
		return true;
	}

	void EditorLayer::RequestNewScene()
	{
		if (!RefreshSceneDirtyState())
		{
			NewScene();
			return;
		}
		m_PendingSceneAction = PendingSceneAction::New;
		m_PendingScenePath.clear();
		m_OpenUnsavedChangesPopup = true;
	}

	void EditorLayer::RequestOpenScene(const std::filesystem::path& path)
	{
		if (path.empty())
			return;
		if (!RefreshSceneDirtyState())
		{
			OpenScene(path);
			return;
		}
		m_PendingSceneAction = PendingSceneAction::Open;
		m_PendingScenePath = path;
		m_OpenUnsavedChangesPopup = true;
	}

	void EditorLayer::RequestExit()
	{
		if (!RefreshSceneDirtyState())
		{
			Application::Get().Close();
			return;
		}
		m_PendingSceneAction = PendingSceneAction::Exit;
		m_PendingScenePath.clear();
		m_OpenUnsavedChangesPopup = true;
	}

	void EditorLayer::ExecutePendingSceneAction()
	{
		const PendingSceneAction action = m_PendingSceneAction;
		const std::filesystem::path path = m_PendingScenePath;
		m_PendingSceneAction = PendingSceneAction::None;
		m_PendingScenePath.clear();
		switch (action)
		{
			case PendingSceneAction::New: NewScene(); break;
			case PendingSceneAction::Open: OpenScene(path); break;
			case PendingSceneAction::Exit: Application::Get().Close(); break;
			case PendingSceneAction::None: break;
		}
	}

	void EditorLayer::RenderUnsavedChangesModal()
	{
		if (m_OpenUnsavedChangesPopup)
		{
			ImGui::OpenPopup("Unsaved Scene Changes");
			m_OpenUnsavedChangesPopup = false;
		}
		if (!ImGui::BeginPopupModal("Unsaved Scene Changes", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize))
			return;

		const std::string sceneName = m_EditorScenePath.empty()
			? "Untitled" : m_EditorScenePath.filename().string();
		ImGui::Text("Save changes to %s?", sceneName.c_str());
		ImGui::TextUnformatted(
			"Unsaved scene changes will be lost if you continue.");
		if (!m_SceneSaveError.empty())
			ImGui::TextColored(ImVec4(0.85f, 0.20f, 0.15f, 1.0f), "%s",
				m_SceneSaveError.c_str());
		ImGui::Separator();
		if (ImGui::Button("Save", ImVec2(90.0f, 0.0f)))
		{
			if (SaveScene())
			{
				ImGui::CloseCurrentPopup();
				ExecutePendingSceneAction();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard", ImVec2(90.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
			ExecutePendingSceneAction();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f)))
		{
			m_PendingSceneAction = PendingSceneAction::None;
			m_PendingScenePath.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	void EditorLayer::RenderSceneSaveError()
	{
		if (m_SceneSaveError.empty()
			|| m_PendingSceneAction != PendingSceneAction::None)
			return;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
			ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
		if (ImGui::Begin("Scene Save Failed", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
		{
			ImGui::TextWrapped("%s", m_SceneSaveError.c_str());
			if (ImGui::Button("Dismiss", ImVec2(-1.0f, 0.0f)))
				m_SceneSaveError.clear();
		}
		ImGui::End();
	}

	bool EditorLayer::RestoreLastScene()
	{
		const auto lastScene = EditorScenePreferences::LoadLastScene(
			std::filesystem::absolute("assets").lexically_normal());
		if (!lastScene)
			return false;
		if (OpenScene(*lastScene))
		{
			GL_CORE_INFO("Restored last editor scene: {0}", lastScene->string());
			return true;
		}

		m_EditorScenePath.clear();
		RememberCurrentScene();
		GL_CORE_WARN("Last editor scene is unavailable; using an empty scene.");
		return false;
	}

	bool EditorLayer::ActivateTemporaryDebugScene(const Ref<Scene>& scene)
	{
		if (!scene || m_SceneState != SceneState::Edit || !m_EditorScene)
			return false;

		PersistEditorCameraState();
		m_TemporaryDebugCameraState = m_EditorCamera.GetState();
		m_ActiveScene = scene;
		m_PostProcessRenderer.ResetHistory();
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
		m_PostProcessRenderer.ResetHistory();
		m_HierarchyPanel.SetContext(m_ActiveScene);
		m_InspectorPanel.SetContext(m_ActiveScene);
		m_HierarchyPanel.SetCommandHistory(&m_CommandHistory);
		m_InspectorPanel.SetCommandHistory(&m_CommandHistory);
		m_HierarchyPanel.SetSelectedEntity({});
		if (m_TemporaryDebugCameraState)
		{
			m_EditorCamera.SetState(*m_TemporaryDebugCameraState);
			m_TemporaryDebugCameraState.reset();
		}
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
		m_PostProcessRenderer.ResetHistory();
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
		m_PostProcessRenderer.ResetHistory();
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

	void EditorLayer::FocusSelectedEntity()
	{
		Entity selectedEntity = m_HierarchyPanel.GetSelectedEntity();
		if (!selectedEntity || !selectedEntity.HasComponent<TransformComponent>())
			return;

		const auto& transformComponent =
			selectedEntity.GetComponent<TransformComponent>();
		const glm::mat4 transform = transformComponent.GetTransform();
		glm::vec3 boundsMin(std::numeric_limits<float>::max());
		glm::vec3 boundsMax(std::numeric_limits<float>::lowest());
		bool hasBounds = false;

		if (selectedEntity.HasComponent<ModelRendererComponent>())
		{
			const auto& modelRenderer =
				selectedEntity.GetComponent<ModelRendererComponent>();
			if (Ref<Model> model = AssetManager::GetModel(modelRenderer.ModelHandle))
			{
				for (const Ref<Mesh>& mesh : model->GetMeshes())
				{
					if (!mesh || !mesh->HasBounds())
						continue;
					EncapsulateTransformedBounds(
						mesh->GetBoundsMin(), mesh->GetBoundsMax(),
						transform, boundsMin, boundsMax);
					hasBounds = true;
				}
			}
		}

		if (selectedEntity.HasComponent<TerrainComponent>())
		{
			const auto& terrain =
				selectedEntity.GetComponent<TerrainComponent>();
			const float halfSize =
				ClampTerrainWorldSize(terrain.Specification.WorldSize) * 0.5f;
			const float minimumHeight =
				std::min(0.0f, terrain.Specification.HeightScale);
			const float maximumHeight =
				std::max(0.0f, terrain.Specification.HeightScale);
			EncapsulateTransformedBounds(
				{ -halfSize, minimumHeight, -halfSize },
				{ halfSize, maximumHeight, halfSize },
				transform, boundsMin, boundsMax);
			hasBounds = true;
		}

		if (!hasBounds)
		{
			const glm::vec3 halfExtents =
				glm::max(glm::abs(transformComponent.Scale) * 0.5f,
					glm::vec3(0.5f));
			boundsMin = transformComponent.Translation - halfExtents;
			boundsMax = transformComponent.Translation + halfExtents;
		}

		const glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
		const glm::vec3 extents = (boundsMax - boundsMin) * 0.5f;
		float radius = glm::length(extents);
		if (!std::isfinite(radius) || radius < 0.5f)
			radius = 0.5f;

		m_EditorCamera.Focus(center, radius * 2.5f);
		m_PostProcessRenderer.ResetHistory();
	}

	void EditorLayer::OnAttach() {
		GL_PROFILE_FUNCTION();
		AssetManager::Initialize("assets");
		const int terrainSamplingVisualMode = GetTerrainSamplingVisualMode();
		m_TerrainCapturePath = GetEnvironmentValue("GLIMMER_TERRAIN_CAPTURE_PATH");
		const uint32_t terrainSynthesisVersion = GetTerrainSynthesisVersion();

		m_ShaderLib.Load("assets/shaders/BalatroVortex.glsl");
		m_ShaderLib.Load("assets/shaders/StarNest.glsl");
		const AssetHandle defaultSkyboxHandle =
			AssetManager::ImportAsset("assets/skyboxes/desert-evening.glsky");
		const AssetHandle defaultInstancingModelHandle =
			AssetManager::ImportAsset("assets/models/geos/Cube.obj");
		const AssetHandle defaultInstancingMaterialHandle =
			AssetManager::ImportAsset("assets/materials/DefaultPBR.glmat");
		const AssetHandle defaultToonMaterialHandle =
			AssetManager::ImportAsset("assets/materials/DefaultToonOutline.glmat");
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
			{ FramebufferTextureFormat::RED_INTEGER }, // 鼠标拾取
			{ FramebufferTextureFormat::RGBA16F },     // 世界空间法线
			{ FramebufferTextureFormat::Depth24Stencil8 }
		};
		m_Framebuffer = Framebuffer::Create(sceneFramebufferSpec);

		m_PostProcessRenderer.Initialize(m_ShaderLib);
		m_PostProcessValidationAutorun = ShouldValidatePostProcess();
		if (m_PostProcessValidationAutorun)
		{
			constexpr std::array<const char*, 9> validationPasses = {
				"assets/shaders/PostProcess/Pixelate.glsl",
				"assets/shaders/PostProcess/Vignette.glsl",
				"assets/shaders/PostProcess/ChromaticAberration.glsl",
				"assets/shaders/PostProcess/WaveDistortion.glsl",
				"assets/shaders/PostProcess/DepthOutline.glsl",
				"assets/shaders/PostProcess/FilmGrain.glsl",
				"assets/shaders/PostProcess/NormalOutline.glsl",
				"assets/shaders/PostProcess/CameraMotionBlur.glsl",
				"assets/shaders/PostProcess/TemporalEcho.glsl"
			};
			bool allPassesAdded = true;
			for (const char* shaderPath : validationPasses)
			{
				if (!m_PostProcessRenderer.AddCustomPass(shaderPath))
				{
					GL_CORE_ERROR(
						"Post-process validation could not add: {0}",
						shaderPath);
					allPassesAdded = false;
					break;
				}
			}
			if (!allPassesAdded)
				Application::Get().Close();
			else
			{
				GL_CORE_INFO(
					"Post-process validation started with {0} sample passes.",
					validationPasses.size());
			}
		}
		m_ShaderLib.Load("assets/shaders/Overlay.glsl");
		m_ShaderLib.Load("Phong", "assets/shaders/Phong.glsl");
		m_ShaderLib.Load("Toon", "assets/shaders/Toon.glsl");
		m_ShaderLib.Load("Toon Surface ABI", "assets/shaders/ToonSurface.glsl");
		m_ShaderLib.Load("Toon Outline ABI", "assets/shaders/ToonOutline.glsl");
		m_ShaderLib.Load("Blinn-Phong", "assets/shaders/BlinnPhong.glsl");
		m_ShaderLib.Load("Hologram", "assets/shaders/Hologram.glsl");

		// Normal startup restores a persisted scene or remains empty. Terrain
		// validation modes own their fixture instead of relying on a demo scene.
		SetEditorScene(CreateRef<Scene>());
		m_UsesTerrainValidationScene =
			ShouldAutorunTerrainSamplingBenchmark()
			|| terrainSamplingVisualMode >= 0
			|| !m_TerrainCapturePath.empty()
			|| ShouldVisualizeTerrainLODs();
		if (m_UsesTerrainValidationScene)
		{
			SetEditorScene(CreateTerrainValidationScene(
				defaultSkyboxHandle,
				terrainShaderHandle,
				terrainGenerationShaderHandle,
				terrainErosionShaderHandle,
				terrainDerivationShaderHandle,
				terrainSynthesisVersion));
			m_EditorScenePath.clear();
			GL_CORE_INFO("Terrain validation fixture scene activated.");
		}
		else
		{
			RestoreLastScene();
		}


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
		m_ContentBrowser.OnFileDoubleClicked = [this](const std::string& path) {
			auto ext = std::filesystem::path(path).extension().string();
			if (ext == ".glimmer")
				RequestOpenScene(path);
			};
		m_ContentBrowser.OnAssetSelected = [this](AssetHandle handle) {
			m_SelectionContext.SelectAsset(handle);
			m_HierarchyPanel.SetSelectedEntity({});
			m_SelectionContext.SelectAsset(handle);
		};

		m_DebugPanel.SetDefaultAssets(
			defaultInstancingModelHandle,
			ShouldAutorunToonLab()
				? defaultToonMaterialHandle : defaultInstancingMaterialHandle,
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
			},
			[this](const glm::vec3& focalPoint, float distance,
				float pitch, float yaw) {
				m_EditorCamera.SetView(focalPoint, distance, pitch, yaw);
			});
		if (ShouldAutorunPBRLab() || ShouldAutorunToonLab())
			m_DebugPanel.GeneratePBRMaterialLabForValidation();
		if (ShouldAutorunShadowVisualValidation())
		{
			ShadowRenderer::SetCascadeDebugVisualization(
				ShouldVisualizeShadowCascades());
			if (!m_DebugPanel.GenerateInstancingLabForShadowVisualValidation(
				ShouldUseShadowCasterCloseup()))
			{
				GL_CORE_ERROR("Shadow Visual Validation autorun could not be initialized.");
				Application::Get().Close();
			}
		}
		m_ShadowBenchmarkAutorun = ShouldAutorunShadowBenchmark();
		if (m_ShadowBenchmarkAutorun
			&& !m_DebugPanel.GenerateInstancingLabForShadowBenchmark())
		{
			GL_CORE_ERROR("Shadow Benchmark autorun could not be initialized.");
			Application::Get().Close();
		}
		m_TerrainSamplingBenchmarkAutorun =
			ShouldAutorunTerrainSamplingBenchmark();
		if (ShouldVisualizeTerrainLODs())
		{
			TerrainRenderer::SetLODVisualizationEnabled(true);
			GL_CORE_INFO("Terrain LOD visualization active: LOD0 red, LOD1 green, LOD2 blue.");
		}
		if (ShouldVisualizeDistanceFog())
		{
			auto& settings = m_PostProcessRenderer.GetSettings();
			settings.DistanceFogEnabled = true;
			settings.FogColor = FogColorSource::SkyLight;
			GL_CORE_INFO("Distance and height fog visualization active with SkyLight color.");
		}
		if (m_TerrainSamplingBenchmarkAutorun)
		{
			m_EditorCamera.SetView({ 0.0f, 10.0f, 0.0f },
				150.0f, -30.0f, 0.0f);
			m_DebugPanel.StartTerrainSamplingBenchmark(true);
			GL_CORE_INFO(
				"Terrain Sampling Benchmark autorun started: fixed camera, 15 warmup and 30 timed samples per mode.");
		}
		else if (terrainSamplingVisualMode >= 0
			&& terrainSamplingVisualMode <= static_cast<int>(
				TerrainRenderer::SamplingMode::AutomaticDistance))
		{
			TerrainRenderer::SetSamplingMode(
				static_cast<TerrainRenderer::SamplingMode>(
					terrainSamplingVisualMode));
			m_EditorCamera.SetView({ 0.0f, 10.0f, 0.0f },
				150.0f, -30.0f, 0.0f);
			GL_CORE_INFO("Terrain Sampling Visual mode active: {0}.",
				terrainSamplingVisualMode);
		}
		else if (!m_TerrainCapturePath.empty())
		{
			TerrainRenderer::SetSamplingMode(
				TerrainRenderer::SamplingMode::AutomaticDistance);
			m_EditorCamera.SetView({ 0.0f, 35.0f, 0.0f },
				650.0f, -30.0f, 35.0f);
			GL_CORE_INFO("Terrain fixed-camera capture armed: synthesis v{0}, path={1}",
				terrainSynthesisVersion, m_TerrainCapturePath.string());
		}


	}
	void EditorLayer::OnDetach() {
		GL_PROFILE_FUNCTION();
		if (m_DebugPanel.IsTemporarySceneActive())
			m_DebugPanel.ExitTemporaryTools();
		if (m_SceneState == SceneState::Play)
			OnSceneStop();
		PersistEditorCameraState();
		RememberCurrentScene();
		m_PostProcessRenderer.Shutdown();
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
				m_PostProcessRenderer.Resize(viewportWidth, viewportHeight);
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
		m_Framebuffer->ClearColorAttachment(2, glm::vec4(0.0f));
		glm::mat4 postProcessViewProjection{ 1.0f };
		glm::vec3 postProcessCameraPosition{ 0.0f };
		bool hasPostProcessCamera = false;

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
				postProcessViewProjection = skyboxProjection * skyboxView;
				postProcessCameraPosition = m_EditorCamera.GetPosition();
				hasPostProcessCamera = true;
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
					postProcessViewProjection = skyboxProjection * skyboxView;
					postProcessCameraPosition = cameraEntity
						.GetComponent<TransformComponent>().Translation;
					hasPostProcessCamera = true;
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
			if (hasSkyboxCamera)
				m_ActiveScene->RenderWaterSurfaces(skyboxProjection * skyboxView,
					postProcessCameraPosition);
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

		PostProcessInput postProcessInput;
		postProcessInput.SceneColorTexture =
			m_Framebuffer->GetColorAttachmentRendererID();
		postProcessInput.SceneDepthTexture =
			m_Framebuffer->GetDepthAttachmentRendererID();
		postProcessInput.SceneNormalTexture =
			m_Framebuffer->GetColorAttachmentRendererID(2);
		postProcessInput.HasCamera = hasPostProcessCamera;
		postProcessInput.CameraPosition = postProcessCameraPosition;
		postProcessInput.InverseViewProjection =
			glm::inverse(postProcessViewProjection);
		if (m_ActiveScene)
		{
			Entity directionalLight = m_ActiveScene->GetDirectionalLightEntity();
			if (directionalLight)
			{
				const auto& light = directionalLight
					.GetComponent<DirectionalLightComponent>();
				postProcessInput.DirectionalLightColor =
					light.Color * std::max(light.Intensity, 0.0f);
			}
			Entity skyLight = m_ActiveScene->GetSkyLightEntity();
			if (skyLight)
			{
				const auto& component = skyLight.GetComponent<SkyLightComponent>();
				if (Ref<Cubemap> cubemap =
					AssetManager::GetCubemap(component.CubemapHandle))
				{
					postProcessInput.SkyLightTexture = cubemap->GetTexture();
					postProcessInput.SkyLightIntensity = component.Intensity;
				}
			}
		}
		m_PostProcessRenderer.Execute(postProcessInput);
		if (!m_TerrainCapturePath.empty() && ++m_TerrainCaptureFrames >= 5)
		{
			std::vector<uint8_t> pixels;
			uint32_t width = 0;
			uint32_t height = 0;
			const bool captured = m_PostProcessRenderer.CaptureOutputRGBA8(
				pixels, width, height)
				&& WriteRgbaBmp(m_TerrainCapturePath, pixels, width, height);
			if (captured)
				GL_CORE_INFO("Terrain fixed-camera capture PASS: {0} ({1}x{2})",
					m_TerrainCapturePath.string(), width, height);
			else
				GL_CORE_ERROR("Terrain fixed-camera capture FAIL: {0}",
					m_TerrainCapturePath.string());
			m_TerrainCapturePath.clear();
			Application::Get().Close();
		}
		if (m_PostProcessValidationAutorun
			&& ++m_PostProcessValidationFrames >= 5)
		{
			const bool temporalInputsReady =
				postProcessInput.SceneNormalTexture != 0
				&& m_PostProcessRenderer.GetVelocityTextureID() != 0
				&& m_PostProcessRenderer.HasValidHistory();
			GL_CORE_ASSERT(temporalInputsReady,
				"Post-process temporal input validation failed.");
			m_PostProcessValidationAutorun = false;
			GL_CORE_INFO(
				"Post-process validation passed: Normal, Velocity and History "
				"are bound across 9 sample passes for 5 rendered frames.");
			Application::Get().Close();
		}
	}

	void EditorLayer::OnImGuiRender() {
		GL_PROFILE_FUNCTION();
		if (Application::GetTime() >= m_NextDirtyCheckTime)
		{
			RefreshSceneDirtyState();
			m_NextDirtyCheckTime = Application::GetTime() + 0.5f;
		}

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
			RequestNewScene();
		}
		if (ImGui::IsKeyChordPressed(
			ImGuiKey_S | ImGuiMod_Ctrl | ImGuiMod_Shift)) {
			SaveSceneAs();
		}
		else if (ImGui::IsKeyChordPressed(ImGuiKey_S | ImGuiMod_Ctrl)) {
			SaveScene();
		}
		if (ImGui::IsKeyChordPressed(ImGuiKey_O | ImGuiMod_Ctrl)) {
			std::string path = FileDialog::OpenFile("Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
			if (!path.empty())
				RequestOpenScene(path);
		}
		if (m_ViewportHovered) {
			if (ImGui::IsKeyPressed(ImGuiKey_1)) m_GizmoType = 0;
			if (ImGui::IsKeyPressed(ImGuiKey_2)) m_GizmoType = 1;
			if (ImGui::IsKeyPressed(ImGuiKey_3)) m_GizmoType = 2;
			if (m_SceneState == SceneState::Edit
				&& !io.WantCaptureKeyboard
				&& ImGui::IsKeyPressed(ImGuiKey_F, false))
				FocusSelectedEntity();
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
					RequestNewScene();
				}
				ImGui::BeginDisabled(m_DebugPanel.IsTemporarySceneActive());
				if (ImGui::MenuItem("Save", "Ctrl+S"))
					SaveScene();
				if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
					SaveSceneAs();
				ImGui::EndDisabled();
				if (ImGui::MenuItem("Open...", "Ctrl+O"))
				{
					std::string path = FileDialog::OpenFile("Glimmer Scene (*.glimmer)\0*.glimmer\0All Files (*.*)\0*.*\0");
					if (!path.empty())
						RequestOpenScene(path);
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Exit")) RequestExit();
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

		// Draw the current inspector before controls that may replace its
		// selection, so active widgets can finish their deactivation transaction.
		m_InspectorPanel.OnImGuiRender();

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
		if (m_ShadowBenchmarkAutorun
			&& m_DebugPanel.IsShadowBenchmarkComplete())
		{
			m_ShadowBenchmarkAutorun = false;
			GL_CORE_INFO("Shadow Benchmark autorun finished; closing the editor.");
			Application::Get().Close();
		}
		if (m_TerrainSamplingBenchmarkAutorun
			&& m_DebugPanel.IsTerrainSamplingBenchmarkComplete())
		{
			m_TerrainSamplingBenchmarkAutorun = false;
			GL_CORE_INFO(
				"Terrain Sampling Benchmark autorun finished; closing the editor.");
			Application::Get().Close();
		}

		// Settings
		ImGui::Begin("Settings");
		auto& postProcessSettings = m_PostProcessRenderer.GetSettings();
		ImGui::SeparatorText("HDR Output");
		ImGui::DragFloat("Exposure (EV)", &postProcessSettings.ExposureEV,
			0.05f, -10.0f, 10.0f, "%+.2f EV");
		ImGui::DragFloat("ACES White Point", &postProcessSettings.ACESWhitePoint,
			0.1f, 1.0f, 32.0f, "%.1f");
		ImGui::SeparatorText("Bloom");
		ImGui::Checkbox("Enabled##Bloom", &postProcessSettings.BloomEnabled);
		if (postProcessSettings.BloomEnabled)
		{
			ImGui::DragFloat("Threshold##Bloom", &postProcessSettings.BloomThreshold,
				0.05f, 0.0f, 20.0f, "%.2f");
			ImGui::DragFloat("Soft Knee##Bloom", &postProcessSettings.BloomKnee,
				0.01f, 0.0f, 1.0f, "%.2f");
			ImGui::DragFloat("Intensity##Bloom", &postProcessSettings.BloomIntensity,
				0.01f, 0.0f, 2.0f, "%.2f");
			ImGui::SliderInt("Blur Passes##Bloom",
				&postProcessSettings.BloomBlurPasses, 1, 12);
		}
		ImGui::Checkbox("Grayscale", &postProcessSettings.GrayscaleEnabled);
		ImGui::SeparatorText("Custom Post Process");
		ImGui::Button("Drop Post Process Shader Here", ImVec2(-1.0f, 0.0f));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload =
				ImGui::AcceptDragDropPayload("SCENE_FILE"))
			{
				const std::string path(
					static_cast<const char*>(payload->Data),
					payload->DataSize - 1);
				if (m_PostProcessRenderer.AddCustomPass(path))
					m_PostProcessPassMessage =
						"Added post-process pass: "
						+ std::filesystem::path(path).stem().string();
				else
					m_PostProcessPassMessage =
						"Could not add pass. Use a valid, non-duplicate .glsl file.";
			}
			ImGui::EndDragDropTarget();
		}
		RenderUnsavedChangesModal();
		RenderSceneSaveError();

		int removeCustomPass = -1;
		int moveCustomPassFrom = -1;
		int moveCustomPassTo = -1;
		const auto& customPasses = m_PostProcessRenderer.GetCustomPasses();
		for (size_t index = 0; index < customPasses.size(); ++index)
		{
			const auto& pass = customPasses[index];
			ImGui::PushID(static_cast<int>(index));
			bool enabled = pass.Enabled;
			if (ImGui::Checkbox("##Enabled", &enabled))
				m_PostProcessRenderer.SetCustomPassEnabled(index, enabled);
			ImGui::SameLine();
			ImGui::TextUnformatted(pass.Name.c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", pass.ShaderPath.string().c_str());
			ImGui::SameLine();
			ImGui::BeginDisabled(index == 0);
			if (ImGui::SmallButton("Up"))
			{
				moveCustomPassFrom = static_cast<int>(index);
				moveCustomPassTo = static_cast<int>(index - 1);
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(index + 1 >= customPasses.size());
			if (ImGui::SmallButton("Down"))
			{
				moveCustomPassFrom = static_cast<int>(index);
				moveCustomPassTo = static_cast<int>(index + 1);
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::SmallButton("Remove"))
				removeCustomPass = static_cast<int>(index);
			ImGui::PopID();
		}
		if (moveCustomPassFrom >= 0)
			m_PostProcessRenderer.MoveCustomPass(
				static_cast<size_t>(moveCustomPassFrom),
				static_cast<size_t>(moveCustomPassTo));
		if (removeCustomPass >= 0)
			m_PostProcessRenderer.RemoveCustomPass(
				static_cast<size_t>(removeCustomPass));
		if (!m_PostProcessPassMessage.empty())
			ImGui::TextWrapped("%s", m_PostProcessPassMessage.c_str());
		ImGui::SeparatorText("Distance Fog");
		ImGui::Checkbox("Enabled##DistanceFog",
			&postProcessSettings.DistanceFogEnabled);
		ImGui::DragFloat("Density##DistanceFog",
			&postProcessSettings.DistanceFogDensity,
			0.001f, 0.0f, 0.1f, "%.3f");
		ImGui::DragFloatRange2("Start / End##DistanceFog",
			&postProcessSettings.DistanceFogStart,
			&postProcessSettings.DistanceFogEnd,
			1.0f, 0.0f, 5000.0f, "Start %.0f", "End %.0f");
		postProcessSettings.DistanceFogEnd = std::max(
			postProcessSettings.DistanceFogEnd,
			postProcessSettings.DistanceFogStart + 1.0f);
		ImGui::ColorEdit3("Color##DistanceFog",
			glm::value_ptr(postProcessSettings.DistanceFogColor));
		const char* fogColorSources[] = {
			"Manual", "Sky Light", "Directional Light"
		};
		int fogColorSource = static_cast<int>(postProcessSettings.FogColor);
		if (ImGui::Combo("Color Source##DistanceFog", &fogColorSource,
			fogColorSources, IM_ARRAYSIZE(fogColorSources)))
			postProcessSettings.FogColor =
				static_cast<FogColorSource>(fogColorSource);
		ImGui::Checkbox("Height Fog", &postProcessSettings.HeightFogEnabled);
		if (postProcessSettings.HeightFogEnabled)
		{
			ImGui::DragFloat("Base Height##DistanceFog",
				&postProcessSettings.HeightFogBaseHeight,
				0.5f, -1000.0f, 1000.0f);
			ImGui::DragFloat("Height Falloff##DistanceFog",
				&postProcessSettings.HeightFogFalloff,
				0.001f, 0.0f, 1.0f, "%.3f");
		}
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

		uint32_t textureID = m_PostProcessRenderer.GetOutputTextureID();
		ImGui::Image((void*)(uintptr_t)textureID, ImVec2{ viewportPanelSize.x, viewportPanelSize.y }, { 0, 1 }, { 1, 0 });

		if (ImGui::BeginDragDropTarget())
		{
			if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
			{
				std::string path((const char*)payload->Data, payload->DataSize - 1);
				auto ext = std::filesystem::path(path).extension().string();
				if (ext == ".glimmer")
				{
					RequestOpenScene(path);
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
				}				else if (ext == ".glsky" || ext == ".hdr")
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
				if (!m_GizmoTransformEdit.IsActive())
				{
					m_GizmoTransformEdit.Begin(tc);
					m_GizmoEditScene = m_ActiveScene;
					m_GizmoEditEntity = selectedEntity.GetUUID();
					m_GizmoEditType = m_GizmoType;
				}
				float t[3], r[3], s[3];
				ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform), t, r, s);
				tc.Translation = { t[0], t[1], t[2] };
				tc.Rotation += glm::vec3(r[0], r[1], r[2]) - tc.Rotation;
				tc.Scale = { s[0], s[1], s[2] };
			}
		}
		if (!ImGuizmo::IsUsing())
			FinishGizmoTransformEdit();

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
		if (event.GetEventType() == EventType::WindowClose)
		{
			if (RefreshSceneDirtyState())
			{
				m_PendingSceneAction = PendingSceneAction::Exit;
				m_PendingScenePath.clear();
				m_OpenUnsavedChangesPopup = true;
				event.Handled = true;
			}
			return;
		}

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
