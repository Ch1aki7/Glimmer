#include "PBRMaterialLabTool.h"

#include "Glimmer/Renderer/ShadowRenderer.h"

#include "Glimmer/Renderer/Model.h"
#include "Glimmer/Scene/SceneSerializer.h"

#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <utility>

namespace gl {

	void PBRMaterialLabTool::SetCallbacks(ActivateSceneCallback activateScene,
		ExitSceneCallback exitScene, SelectEntityCallback selectEntity)
	{
		m_ActivateScene = std::move(activateScene);
		m_ExitScene = std::move(exitScene);
		m_SelectEntity = std::move(selectEntity);
	}

	void PBRMaterialLabTool::SetDefaultAssets(AssetHandle sphereModel,
		AssetHandle material, AssetHandle skybox, AssetHandle normalTexture,
		AssetHandle aoTexture, AssetHandle emissiveTexture)
	{
		m_SphereModel = sphereModel;
		m_Material = material;
		m_Skybox = skybox;
		m_NormalTexture = normalTexture;
		m_AOTexture = aoTexture;
		m_EmissiveTexture = emissiveTexture;
	}

	bool PBRMaterialLabTool::DrawAssetTarget(
		const char* label, AssetType type, AssetHandle& handle)
	{
		const AssetMetadata metadata = AssetManager::GetMetadata(handle);
		const std::string value = metadata.IsValid()
			? metadata.FilePath.filename().string() : "None (drop asset)";
		ImGui::Text("%s: %s", label, value.c_str());
		if (!ImGui::BeginDragDropTarget())
			return false;

		bool changed = false;
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
		{
			const std::string path(
				static_cast<const char*>(payload->Data), payload->DataSize - 1);
			const AssetHandle dropped = AssetManager::ImportAsset(path);
			if (AssetManager::GetMetadata(dropped).Type == type)
			{
				handle = dropped;
				changed = true;
			}
		}
		ImGui::EndDragDropTarget();
		return changed;
	}

	bool PBRMaterialLabTool::Generate()
	{
		const Ref<Model> sphereModel = AssetManager::GetModel(m_SphereModel);
		if (!m_ActivateScene || !sphereModel || sphereModel->GetMeshes().empty()
			|| !AssetManager::GetMaterial(m_Material))
		{
			m_Status = "Select a valid sphere model and material.";
			m_Succeeded = false;
			return false;
		}

		if (m_Active)
			Exit();
		Ref<Scene> scene = CreateRef<Scene>();
		Entity sun = scene->CreateEntity("PBR Lab Sun");
		auto& light = sun.AddComponent<DirectionalLightComponent>();
		light.Intensity = 3.0f;
		light.AmbientIntensity = 0.18f;
		sun.GetComponent<TransformComponent>().Rotation = { -40.0f, 25.0f, 0.0f };
		if (AssetManager::GetMetadata(m_Skybox).Type == AssetType::Cubemap)
		{
			Entity sky = scene->CreateEntity("PBR Lab Sky Light");
			sky.AddComponent<SkyLightComponent>(m_Skybox);
		}

		const char* names[] = {
			"Normal Map", "Ambient Occlusion", "Emissive",
			"Dielectric Smooth", "Metal Smooth", "Metal Rough"
		};
		m_Entities.clear();
		for (uint32_t index = 0; index < 6; ++index)
		{
			Entity entity = scene->CreateEntity(names[index]);
			entity.GetComponent<TransformComponent>().Translation = m_Origin + glm::vec3{
				(static_cast<float>(index) - 2.5f) * m_Spacing, 0.0f, 0.0f };
			entity.AddComponent<ModelRendererComponent>(m_SphereModel);
			auto& overrides = entity.AddComponent<MaterialComponent>(m_Material).Overrides;
			overrides.Values.AlphaMode = MaterialAlphaMode::Opaque;
			overrides.SetEnabled(MaterialOverride::AlphaMode, true);

			if (index == 0)
			{
				overrides.Values.NormalTexture = m_NormalTexture;
				overrides.Values.NormalScale = 1.0f;
				overrides.SetEnabled(MaterialOverride::NormalTexture, true);
				overrides.SetEnabled(MaterialOverride::NormalScale, true);
			}
			else if (index == 1)
			{
				overrides.Values.AOTexture = m_AOTexture;
				overrides.Values.AOStrength = 1.0f;
				overrides.SetEnabled(MaterialOverride::AOTexture, true);
				overrides.SetEnabled(MaterialOverride::AOStrength, true);
			}
			else if (index == 2)
			{
				overrides.Values.EmissiveTexture = m_EmissiveTexture;
				overrides.Values.EmissiveColor = { 1.0f, 0.65f, 0.25f };
				overrides.Values.EmissiveStrength = 2.5f;
				overrides.SetEnabled(MaterialOverride::EmissiveTexture, true);
				overrides.SetEnabled(MaterialOverride::EmissiveColor, true);
				overrides.SetEnabled(MaterialOverride::EmissiveStrength, true);
			}
			else
			{
				overrides.Values.Metallic = index == 3 ? 0.0f : 1.0f;
				overrides.Values.Roughness = index == 5 ? 0.8f : 0.15f;
				overrides.SetEnabled(MaterialOverride::Metallic, true);
				overrides.SetEnabled(MaterialOverride::Roughness, true);
			}
			m_Entities.push_back(entity.GetUUID());
		}

		if (!m_ActivateScene(scene))
		{
			m_Status = "The editor rejected the temporary PBR lab scene.";
			m_Succeeded = false;
			return false;
		}
		m_Scene = std::move(scene);
		m_ExpectedItems = 6u * static_cast<uint32_t>(sphereModel->GetMeshes().size());
		m_ValidationLogged = false;
		m_Active = true;
		m_Succeeded = true;
		m_Status = "PBR channel scene active. Frame the six spheres in the viewport.";
		return true;
	}

	bool PBRMaterialLabTool::RunSerializationValidation() const
	{
		if (!m_Scene || m_Entities.empty())
			return false;
		const std::filesystem::path temporaryDirectory =
			std::filesystem::temp_directory_path();
		const std::filesystem::path materialPath =
			temporaryDirectory / "Glimmer-PBR-Material-RoundTrip.glmat";
		const std::filesystem::path scenePath =
			temporaryDirectory / "Glimmer-PBR-Scene-RoundTrip.glimmer";
		std::error_code error;
		std::filesystem::remove(materialPath, error);
		error.clear();
		std::filesystem::remove(scenePath, error);

		bool passed = false;
		{
			std::ofstream legacyMaterial(materialPath, std::ios::binary | std::ios::trunc);
			legacyMaterial << "Material:\n  Shader: 0\n";
			legacyMaterial.close();
			Ref<Material> material = Material::Create(materialPath);
			if (material)
			{
				MaterialState expected = material->GetState();
				expected.Properties.BaseColor = { 0.8f, 0.7f, 0.6f, 0.5f };
				expected.Properties.BaseColorTexture = m_EmissiveTexture;
				expected.Properties.NormalTexture = m_NormalTexture;
				expected.Properties.AOTexture = m_AOTexture;
				expected.Properties.EmissiveTexture = m_EmissiveTexture;
				expected.Properties.TilingFactor = 2.0f;
				expected.Properties.Metallic = 0.75f;
				expected.Properties.Roughness = 0.25f;
				expected.Properties.NormalScale = 1.25f;
				expected.Properties.AOStrength = 0.65f;
				expected.Properties.EmissiveColor = { 0.9f, 0.4f, 0.2f };
				expected.Properties.EmissiveStrength = 3.0f;
				expected.Properties.AlphaMode = MaterialAlphaMode::Mask;
				expected.Properties.AlphaCutoff = 0.35f;
				material->SetState(expected);
				if (material->Save())
				{
					const Ref<Material> reloaded = Material::Create(materialPath);
					passed = reloaded && reloaded->GetState() == expected;
				}
			}
		}

		if (passed)
		{
			SceneSerializer(m_Scene).Serialize(scenePath.string());
			Ref<Scene> reloadedScene = CreateRef<Scene>();
			SceneSerializer serializer(reloadedScene);
			passed = serializer.Deserialize(scenePath.string());
			for (const UUID uuid : m_Entities)
			{
				const Entity source = m_Scene->FindEntityByUUID(uuid);
				const Entity reloaded = reloadedScene->FindEntityByUUID(uuid);
				passed = passed && source && reloaded
					&& source.HasComponent<MaterialComponent>()
					&& reloaded.HasComponent<MaterialComponent>()
					&& source.GetComponent<MaterialComponent>().MaterialHandle
						== reloaded.GetComponent<MaterialComponent>().MaterialHandle
					&& source.GetComponent<MaterialComponent>().Overrides
						== reloaded.GetComponent<MaterialComponent>().Overrides;
			}
		}

		std::filesystem::remove(materialPath, error);
		error.clear();
		std::filesystem::remove(scenePath, error);
		return passed;
	}

	bool PBRMaterialLabTool::GenerateForValidation()
	{
		if (!Generate())
			return false;
		const bool serializationPassed = RunSerializationValidation();
		if (serializationPassed)
			GL_CORE_INFO("PBR Material/YAML roundtrip PASS: legacy defaults and all channel overrides restored.");
		else
			GL_CORE_ERROR("PBR Material/YAML roundtrip FAIL.");
		return serializationPassed;
	}

	void PBRMaterialLabTool::Exit()
	{
		if (!m_Active)
			return;
		if (m_ExitScene)
			m_ExitScene();
		m_Scene.reset();
		m_Entities.clear();
		m_Active = false;
		m_Status = "PBR material lab exited; editor scene restored.";
		m_Succeeded = true;
	}

	void PBRMaterialLabTool::OnImGuiRender(
		const Renderer3D::Statistics& statistics, bool anotherTemporaryToolActive)
	{
		ImGui::SeparatorText("PBR Material Lab");
		ImGui::TextWrapped("Creates six temporary material spheres for Normal, AO, Emissive and Metallic/Roughness comparison.");
		DrawAssetTarget("Sphere Model", AssetType::Model, m_SphereModel);
		DrawAssetTarget("Material", AssetType::Material, m_Material);
		if (DrawAssetTarget("Normal Texture (Linear)", AssetType::Texture2D, m_NormalTexture))
			AssetManager::SetTextureMetadata(m_NormalTexture,
				TextureColorSpace::Linear, TextureSemantic::Normal);
		if (DrawAssetTarget("AO Texture (Linear)", AssetType::Texture2D, m_AOTexture))
			AssetManager::SetTextureMetadata(m_AOTexture,
				TextureColorSpace::Linear, TextureSemantic::Data);
		if (DrawAssetTarget("Emissive Texture (sRGB)", AssetType::Texture2D, m_EmissiveTexture))
			AssetManager::SetTextureMetadata(m_EmissiveTexture,
				TextureColorSpace::SRGB, TextureSemantic::Color);
		ImGui::DragFloat("Sphere Spacing", &m_Spacing, 0.1f, 1.0f, 10.0f);
		ImGui::DragFloat3("Lab Origin##PBR", &m_Origin.x, 0.1f);

		ImGui::BeginDisabled(anotherTemporaryToolActive);
		if (ImGui::Button(m_Active ? "Regenerate PBR Lab" : "Generate PBR Lab"))
			Generate();
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!m_Active);
		if (ImGui::Button("Exit PBR Lab"))
			Exit();
		ImGui::EndDisabled();

		const ImVec4 color = m_Succeeded
			? ImVec4(0.35f, 0.85f, 0.45f, 1.0f)
			: ImVec4(0.95f, 0.35f, 0.30f, 1.0f);
		ImGui::TextColored(color, "%s", m_Status.c_str());
		if (anotherTemporaryToolActive)
			ImGui::TextDisabled("Exit the active Instancing Lab before generating this scene.");

		if (m_Active && m_Scene && m_SelectEntity)
		{
			const bool currentFrame = statistics.SubmittedItems == m_ExpectedItems;
			const bool passed = currentFrame
				&& statistics.RenderedItems == m_ExpectedItems
				&& statistics.SkippedModels == 0;
			const ImVec4 validationColor = !currentFrame
				? ImVec4(0.95f, 0.75f, 0.25f, 1.0f)
				: passed ? ImVec4(0.35f, 0.85f, 0.45f, 1.0f)
				: ImVec4(0.95f, 0.35f, 0.30f, 1.0f);
			ImGui::TextColored(validationColor, "Render validation: %s (%u/%u items)",
				!currentFrame ? "PENDING" : passed ? "PASS" : "FAIL",
				statistics.RenderedItems, m_ExpectedItems);
			for (size_t index = 0; index < m_Entities.size(); ++index)
			{
				if (index > 0)
					ImGui::SameLine();
				ImGui::PushID(static_cast<int>(index));
				if (ImGui::SmallButton(std::to_string(index + 1).c_str()))
					m_SelectEntity(m_Scene->FindEntityByUUID(m_Entities[index]));
				ImGui::PopID();
			}
			ImGui::TextDisabled("1 Normal, 2 AO, 3 Emissive, 4 Dielectric, 5 Metal Smooth, 6 Metal Rough");
		}
	}

	void PBRMaterialLabTool::UpdateValidation(
		const Renderer3D::Statistics& statistics)
	{
		if (!m_Active || m_ValidationLogged || m_ExpectedItems == 0)
			return;
		if (statistics.SubmittedItems == m_ExpectedItems
			&& statistics.RenderedItems == m_ExpectedItems
			&& statistics.SkippedModels == 0)
		{
			GL_CORE_INFO("PBR Material Lab PASS: rendered {0}/{1} items with no skipped models.",
				statistics.RenderedItems, m_ExpectedItems);
			const ShadowRenderer::Statistics shadowStatistics =
				ShadowRenderer::GetStatistics();
			GL_CORE_INFO(
				"Shadow Frustum validation: {0} candidates, {1} rendered, {2} culled across {3} cascades.",
				shadowStatistics.CandidateDraws,
				shadowStatistics.RenderedDraws,
				shadowStatistics.CulledDraws,
				shadowStatistics.CascadePasses);
			m_ValidationLogged = true;
		}
	}

}
