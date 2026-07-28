#include "SceneHierarchyPanel.h"
#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/Material.h"
#include "Glimmer/Renderer/TerrainRenderer.h"
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace gl {
	void SceneHierarchyPanel::SetSelectedEntity(Entity entity)
	{
		m_SelectionContext = entity;
		if (m_SharedSelection)
			m_SharedSelection->SelectEntity(entity);
	}


	void SceneHierarchyPanel::OnImGuiRender()
	{
		if (!m_Context) {
			ImGui::TextDisabled("No scene context");
			return;
		}

		ImGui::Begin("Scene Hierarchy");

		if (ImGui::Button("+ Create Entity"))
		{
			auto entity = m_Context->CreateEntity();
			SetSelectedEntity(entity);
			if (OnEntitySelected) OnEntitySelected(entity);
		}

		ImGui::Separator();

		uint32_t idCounter = 0;
		m_Context->m_Registry.view<entt::entity>().each([&](entt::entity handle) {
			Entity entity{ handle, m_Context.get() };
			if (entity.HasComponent<TagComponent>()) {
				DrawEntityNode(entity, idCounter);
			}
		});

		ImGui::Separator();

		if (m_ShowDeletePopup) {
			ImGui::OpenPopup("Delete Entity?");
			m_ShowDeletePopup = false;
		}

		if (ImGui::BeginPopupModal("Delete Entity?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (m_RightClickedEntity && m_RightClickedEntity.HasComponent<TagComponent>()) {
				ImGui::Text("Delete '%s'?", m_RightClickedEntity.GetComponent<TagComponent>().Tag.c_str());
			}
			ImGui::Separator();

			if (ImGui::Button("Yes", ImVec2(80, 0))) {
				if (m_SelectionContext == m_RightClickedEntity)
					SetSelectedEntity({});

				if (OnEntityDeleted)
					OnEntityDeleted(m_RightClickedEntity);

				m_Context->DestroyEntity(m_RightClickedEntity);
				m_RightClickedEntity = Entity{};
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No", ImVec2(80, 0))) {
				m_RightClickedEntity = Entity{};
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::End();

	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity, uint32_t& idCounter)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		std::string label = tag.empty() ? "Unnamed" : tag;

		std::string badges;
		if (entity.HasComponent<CameraComponent>())          badges += " [Cam]";
		if (entity.HasComponent<SpriteRendererComponent>())  badges += " [Spr]";
		if (entity.HasComponent<ModelRendererComponent>())   badges += " [Model]";
		if (entity.HasComponent<MaterialComponent>())        badges += " [Mat]";
		if (entity.HasComponent<TerrainComponent>())         badges += " [Terrain]";
		if (entity.HasComponent<DirectionalLightComponent>()) badges += " [Sun]";
		if (entity.HasComponent<PointLightComponent>())       badges += " [Point]";
		if (entity.HasComponent<SkyLightComponent>())         badges += " [Sky]";
		if (entity.HasComponent<NativeScriptComponent>())    badges += " [Scr]";
		label += badges;

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf
			| ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_NoTreePushOnOpen;

		if (m_SelectionContext == entity)
			flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", label.c_str());

		if (ImGui::IsItemClicked()) {
			SetSelectedEntity(entity);
			if (OnEntitySelected) OnEntitySelected(entity);
		}

		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Duplicate")) {
				SetSelectedEntity(m_Context->DuplicateEntity(entity));
				if (OnEntitySelected) OnEntitySelected(m_SelectionContext);
			}
			if (ImGui::MenuItem("Delete")) {
				m_RightClickedEntity = entity;
				m_ShowDeletePopup = true;
			}
			ImGui::EndPopup();
		}

		(void)idCounter;
	}

	void SceneHierarchyPanel::DrawComponents(Entity entity)
	{
		// --- Tag ---
		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strncpy(buffer, tag.c_str(), sizeof(buffer) - 1);
			if (ImGui::InputText("Tag", buffer, sizeof(buffer)))
			{
				tag = std::string(buffer);
			}
		}

		DrawComponent<TransformComponent>("Transform", entity,
			[this, entity](TransformComponent& transform) {
				auto drawVector = [this, entity, &transform](
					const char* label, glm::vec3& value, float speed,
					float minimum = 0.0f, float maximum = 0.0f) {
					const TransformComponent valueBeforeWidget = transform;
					ImGui::DragFloat3(label, glm::value_ptr(value), speed, minimum, maximum);

					if (ImGui::IsItemActivated())
						m_TransformBeforeEdit = valueBeforeWidget;

					if (ImGui::IsItemDeactivatedAfterEdit()
						&& m_TransformBeforeEdit && m_CommandHistory && m_Context)
					{
						const TransformComponent before = *m_TransformBeforeEdit;
						const TransformComponent after = transform;
						const Ref<Scene> scene = m_Context;
						const UUID uuid = entity.GetUUID();
						m_CommandHistory->PushExecuted(
							std::make_unique<LambdaEditorCommand>(
								std::string("Edit Transform ") + label,
								[scene, uuid, after]() {
									Entity target = scene->FindEntityByUUID(uuid);
									if (target && target.HasComponent<TransformComponent>())
										target.GetComponent<TransformComponent>() = after;
								},
								[scene, uuid, before]() {
									Entity target = scene->FindEntityByUUID(uuid);
									if (target && target.HasComponent<TransformComponent>())
										target.GetComponent<TransformComponent>() = before;
								}));
						m_TransformBeforeEdit.reset();
					}
				};

				drawVector("Position", transform.Translation, 0.1f);
				drawVector("Rotation", transform.Rotation, 1.0f);
				drawVector("Scale", transform.Scale, 0.05f, 0.01f, 10.0f);
			},
			false);

		// --- Terrain ---
		if (entity.HasComponent<TerrainComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(TerrainComponent).hash_code(),
				ImGuiTreeNodeFlags_DefaultOpen, "Terrain"))
			{
				auto& terrain = entity.GetComponent<TerrainComponent>();
				auto& spec = terrain.Specification;
				bool regenerate = false;
				regenerate |= ImGui::Checkbox("Procedural", &spec.Procedural);
				int heightResolution = static_cast<int>(spec.HeightMapResolution);
				if (ImGui::InputInt("Height Resolution", &heightResolution, 0))
				{
					spec.HeightMapResolution = static_cast<uint32_t>(std::clamp(heightResolution, 64, 2048));
					terrain.Runtime.reset();
				}
				int meshResolution = static_cast<int>(spec.MeshResolution);
				if (ImGui::InputInt("Mesh Resolution", &meshResolution, 0))
				{
					spec.MeshResolution = static_cast<uint32_t>(std::clamp(meshResolution, 16, 512));
					terrain.Runtime.reset();
				}
				regenerate |= ImGui::DragFloat("Height Scale", &spec.HeightScale, 0.1f, 0.0f, 500.0f);

				if (spec.Procedural)
				{
					auto& noise = spec.Noise;
					regenerate |= ImGui::DragInt("Seed", &noise.Seed, 1.0f);
					regenerate |= ImGui::SliderInt("Octaves", &noise.Octaves, 1, 12);
					regenerate |= ImGui::DragFloat("Frequency", &noise.Frequency, 0.01f, 0.05f, 32.0f);
					regenerate |= ImGui::DragFloat("Lacunarity", &noise.Lacunarity, 0.01f, 1.0f, 4.0f);
					regenerate |= ImGui::DragFloat("Persistence", &noise.Persistence, 0.01f, 0.05f, 0.95f);
					regenerate |= ImGui::DragFloat("Domain Warp", &noise.DomainWarp, 0.01f, 0.0f, 4.0f);
					regenerate |= ImGui::SliderFloat("Ridge Strength", &noise.RidgeStrength, 0.0f, 1.0f);
					regenerate |= ImGui::SliderFloat("Continent Scale", &noise.ContinentScale, 0.05f, 1.0f);
					regenerate |= ImGui::SliderFloat("Erosion Strength", &noise.ErosionStrength, 0.0f, 0.5f);
					regenerate |= ImGui::SliderFloat("Detail Strength", &noise.DetailStrength, 0.0f, 0.25f);
					regenerate |= ImGui::DragFloat2("Offset", &noise.Offset.x, 0.005f);
				}
				else
				{
					const AssetMetadata metadata = AssetManager::GetMetadata(spec.HeightMapHandle);
					ImGui::Text("Height Map: %s", metadata.IsValid()
						? metadata.FilePath.filename().string().c_str() : "None (drag image here)");
					if (ImGui::BeginDragDropTarget())
					{
						if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
						{
							std::string path((const char*)payload->Data, payload->DataSize - 1);
							AssetHandle handle = AssetManager::ImportAsset(path);
							if (AssetManager::GetMetadata(handle).Type == AssetType::Texture2D)
							{
								spec.HeightMapHandle = handle;
								terrain.Runtime.reset();
							}
						}
						ImGui::EndDragDropTarget();
					}
				}
				if (regenerate)
					TerrainRenderer::Invalidate(terrain);
				if (ImGui::Button("Regenerate"))
					TerrainRenderer::Invalidate(terrain);
				ImGui::TreePop();
			}
		}
		// --- Directional Light ---
		if (entity.HasComponent<DirectionalLightComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(DirectionalLightComponent).hash_code(),
				ImGuiTreeNodeFlags_DefaultOpen, "Directional Light"))
			{
				auto& light = entity.GetComponent<DirectionalLightComponent>();
				ImGui::Checkbox("Enabled##Directional", &light.Enabled);
				ImGui::ColorEdit3("Color##Directional", glm::value_ptr(light.Color));
				ImGui::DragFloat("Intensity##Directional", &light.Intensity,
					0.05f, 0.0f, 100.0f);
				ImGui::DragFloat("Ambient##Directional", &light.AmbientIntensity,
					0.01f, 0.0f, 10.0f);
				ImGui::TextDisabled("Direction follows Transform rotation.");
				ImGui::TreePop();
			}
		}

		// --- Point Light ---
		if (entity.HasComponent<PointLightComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(PointLightComponent).hash_code(),
				ImGuiTreeNodeFlags_DefaultOpen, "Point Light"))
			{
				auto& light = entity.GetComponent<PointLightComponent>();
				ImGui::Checkbox("Enabled##Point", &light.Enabled);
				ImGui::ColorEdit3("Color##Point", glm::value_ptr(light.Color));
				ImGui::DragFloat("Intensity##Point", &light.Intensity,
					0.1f, 0.0f, 1000.0f);
				ImGui::DragFloat("Range##Point", &light.Range,
					0.1f, 0.01f, 1000.0f);
				ImGui::TreePop();
			}
		}
		// --- Sky Light ---
		if (entity.HasComponent<SkyLightComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(SkyLightComponent).hash_code(),
				ImGuiTreeNodeFlags_DefaultOpen, "Sky Light"))
			{
				auto& skyLight = entity.GetComponent<SkyLightComponent>();
				const AssetMetadata metadata =
					AssetManager::GetMetadata(skyLight.CubemapHandle);
				const bool hasCubemap = metadata.IsValid()
					&& metadata.Type == AssetType::Cubemap;
				const std::string assetName = hasCubemap
					? metadata.FilePath.filename().string()
					: "None (drag .glsky here)";

				ImGui::Checkbox("Enabled##SkyLight", &skyLight.Enabled);
				ImGui::DragFloat("Intensity##SkyLight", &skyLight.Intensity,
					0.05f, 0.0f, 20.0f);
				ImGui::Text("Cubemap: %s", assetName.c_str());
				ImGui::SameLine();
				if (hasCubemap && ImGui::SmallButton("X##SkyLight"))
					skyLight.CubemapHandle = AssetHandle(0);

				if (ImGui::BeginDragDropTarget())
				{
					if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
					{
						std::string path(
							(const char*)payload->Data,
							payload->DataSize - 1);
						AssetHandle handle = AssetManager::ImportAsset(path);
						if (AssetManager::GetMetadata(handle).Type
							== AssetType::Cubemap)
							skyLight.CubemapHandle = handle;
					}
					ImGui::EndDragDropTarget();
				}
				ImGui::TreePop();
			}
		}
		// --- Camera ---
		if (entity.HasComponent<CameraComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(CameraComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Camera"))
			{
				auto& cameraComponent = entity.GetComponent<CameraComponent>();
				auto& camera = cameraComponent.Camera;

				ImGui::Checkbox("Primary", &cameraComponent.Primary);

				const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
				const char* currentProjectionTypeString = projectionTypeStrings[(int)camera.GetProjectionType()];
				if (ImGui::BeginCombo("Projection", currentProjectionTypeString))
				{
					for (int i = 0; i < 2; i++)
					{
						bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
						if (ImGui::Selectable(projectionTypeStrings[i], isSelected))
						{
							currentProjectionTypeString = projectionTypeStrings[i];
							camera.SetProjectionType((SceneCamera::ProjectionType)i);
						}
						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
				{
					float perspectiveVerticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
					if (ImGui::DragFloat("Vertical FOV", &perspectiveVerticalFov, 0.5f, 1.0f, 179.0f))
						camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));

					float perspectiveNear = camera.GetPerspectiveNearClip();
					if (ImGui::DragFloat("Near", &perspectiveNear, 0.01f, 0.001f))
						camera.SetPerspectiveNearClip(perspectiveNear);

					float perspectiveFar = camera.GetPerspectiveFarClip();
					if (ImGui::DragFloat("Far", &perspectiveFar, 1.0f))
						camera.SetPerspectiveFarClip(perspectiveFar);
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
				{
					float orthoSize = camera.GetOrthographicSize();
					if (ImGui::DragFloat("Size", &orthoSize, 0.1f, 0.1f))
						camera.SetOrthographicSize(orthoSize);

					float orthoNear = camera.GetOrthographicNearClip();
					if (ImGui::DragFloat("Near", &orthoNear, 0.1f))
						camera.SetOrthographicNearClip(orthoNear);

					float orthoFar = camera.GetOrthographicFarClip();
					if (ImGui::DragFloat("Far", &orthoFar, 0.1f))
						camera.SetOrthographicFarClip(orthoFar);

					ImGui::Checkbox("Fixed Aspect Ratio", &cameraComponent.FixedAspectRatio);
				}

				ImGui::TreePop();
			}
		}

		// --- Model Renderer ---
		if (entity.HasComponent<ModelRendererComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(ModelRendererComponent).hash_code(),
				ImGuiTreeNodeFlags_DefaultOpen, "Model Renderer"))
			{
				auto& component = entity.GetComponent<ModelRendererComponent>();
				AssetMetadata metadata = AssetManager::GetMetadata(component.ModelHandle);
				const bool hasModel = metadata.IsValid() && metadata.Type == AssetType::Model;
				const std::string modelName = hasModel
					? metadata.FilePath.filename().string()
					: "None (drag .obj here)";
				ImGui::Text("Model: %s", modelName.c_str());
				ImGui::SameLine();
				if (hasModel && ImGui::SmallButton("X##Model"))
					component.ModelHandle = AssetHandle(0);

				if (ImGui::BeginDragDropTarget())
				{
					if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
					{
						std::string path((const char*)payload->Data, payload->DataSize - 1);
						AssetHandle handle = AssetManager::ImportAsset(path);
						if (AssetManager::GetMetadata(handle).Type == AssetType::Model)
							component.ModelHandle = handle;
					}
					ImGui::EndDragDropTarget();
				}
				ImGui::TreePop();
			}
		}
		// --- Sprite Renderer ---
		if (entity.HasComponent<SpriteRendererComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(SpriteRendererComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Sprite Renderer"))
			{
				auto& src = entity.GetComponent<SpriteRendererComponent>();
				ImGui::ColorEdit4("Color", glm::value_ptr(src.Color));
				ImGui::DragFloat("Tiling", &src.TilingFactor, 0.1f, 0.1f, 10.0f);

				// 纹理状态 + 预览
				const bool hasTexture = AssetManager::IsAssetHandleValid(src.TextureHandle);
				const AssetMetadata textureMetadata = AssetManager::GetMetadata(src.TextureHandle);
				const std::string textureName = hasTexture
					? textureMetadata.FilePath.filename().string()
					: "None (drag here)";
				ImGui::Text("Texture: %s", textureName.c_str());
				ImGui::SameLine();
				if (hasTexture && ImGui::SmallButton("X"))
					src.TextureHandle = AssetHandle(0);

				// 接收从 Content Browser 拖来的贴图文件
				if (ImGui::BeginDragDropTarget())
				{
					if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
					{
						std::string path((const char*)payload->Data, payload->DataSize - 1);
						std::string ext = std::filesystem::path(path).extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(),
							[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
						if (ext == ".png" || ext == ".jpg" || ext == ".jpeg"
							|| ext == ".tga" || ext == ".bmp")
							src.TextureHandle = AssetManager::ImportAsset(path);
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::TreePop();
			}
		}
		// --- Material ---
		if (entity.HasComponent<MaterialComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(MaterialComponent).hash_code(),
				ImGuiTreeNodeFlags_DefaultOpen, "Material"))
			{
				auto& component = entity.GetComponent<MaterialComponent>();
				AssetMetadata metadata = AssetManager::GetMetadata(component.MaterialHandle);
				const bool hasMaterial = metadata.IsValid()
					&& metadata.Type == AssetType::Material;

				const std::string materialName = hasMaterial
					? metadata.FilePath.filename().string()
					: "None (drag .glmat here)";
				ImGui::Text("Asset: %s", materialName.c_str());
				ImGui::SameLine();
				if (hasMaterial && ImGui::SmallButton("X##Material"))
					component.MaterialHandle = AssetHandle(0);

				if (ImGui::BeginDragDropTarget())
				{
					if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
					{
						std::string path((const char*)payload->Data, payload->DataSize - 1);
						std::string extension = std::filesystem::path(path).extension().string();
						std::transform(extension.begin(), extension.end(), extension.begin(),
							[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
						if (extension == ".glmat")
							component.MaterialHandle = AssetManager::ImportAsset(path);
					}
					ImGui::EndDragDropTarget();
				}

				if (Ref<Material> material = AssetManager::GetMaterial(component.MaterialHandle))
				{
					auto& properties = material->GetProperties();
					bool changed = false;

					AssetMetadata shaderMetadata =
						AssetManager::GetMetadata(material->GetShaderHandle());
					const bool hasShader = shaderMetadata.IsValid()
						&& shaderMetadata.Type == AssetType::Shader;
					const std::string shaderName = hasShader
						? shaderMetadata.FilePath.filename().string()
						: "None (drag .glsl here)";
					ImGui::Text("Shader: %s", shaderName.c_str());
					ImGui::SameLine();
					if (hasShader && ImGui::SmallButton("X##MaterialShader"))
					{
						material->SetShaderHandle(AssetHandle(0));
						changed = true;
					}
					if (ImGui::BeginDragDropTarget())
					{
						if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
						{
							std::string path((const char*)payload->Data, payload->DataSize - 1);
							AssetHandle handle = AssetManager::ImportAsset(path);
							if (AssetManager::GetMetadata(handle).Type == AssetType::Shader)
							{
								material->SetShaderHandle(handle);
								changed = true;
							}
						}
						ImGui::EndDragDropTarget();
					}
					changed |= ImGui::ColorEdit4("Base Color", glm::value_ptr(properties.BaseColor));
					changed |= ImGui::DragFloat("Material Tiling", &properties.TilingFactor,
						0.05f, 0.01f, 100.0f);
					changed |= ImGui::SliderFloat("Metallic", &properties.Metallic, 0.0f, 1.0f);
					changed |= ImGui::SliderFloat("Roughness", &properties.Roughness, 0.04f, 1.0f);

					AssetMetadata textureMetadata =
						AssetManager::GetMetadata(properties.BaseColorTexture);
					const bool hasBaseColorTexture = textureMetadata.IsValid()
						&& textureMetadata.Type == AssetType::Texture2D;
					const std::string textureName = hasBaseColorTexture
						? textureMetadata.FilePath.filename().string()
						: "None (drag image here)";
					ImGui::Text("Base Color Texture: %s", textureName.c_str());
					ImGui::SameLine();
					if (hasBaseColorTexture && ImGui::SmallButton("X##MaterialTexture"))
					{
						properties.BaseColorTexture = AssetHandle(0);
						changed = true;
					}

					if (ImGui::BeginDragDropTarget())
					{
						if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
						{
							std::string path((const char*)payload->Data, payload->DataSize - 1);
							AssetHandle textureHandle = AssetManager::ImportAsset(path);
							AssetMetadata droppedMetadata = AssetManager::GetMetadata(textureHandle);
							if (droppedMetadata.Type == AssetType::Texture2D)
							{
								properties.BaseColorTexture = textureHandle;
								changed = true;
							}
						}
						ImGui::EndDragDropTarget();
					}

					if (changed && !material->Save())
						GL_CORE_ERROR("Failed to save material: {0}", material->GetPath().string());
				}

				ImGui::TreePop();
			}
		}
		ImGui::Spacing();
		ImGui::Separator();
		const float buttonWidth = std::min(220.0f, ImGui::GetContentRegionAvail().x);
		if (ImGui::Button("Add Component", ImVec2(buttonWidth, 0.0f)))
			ImGui::OpenPopup("AddComponentPopup");
		DrawAddComponentMenu(entity);
	}

	void SceneHierarchyPanel::DrawAddComponentMenu(Entity entity)
	{
		if (!ImGui::BeginPopup("AddComponentPopup"))
			return;

		bool hasAvailableComponent = false;
		auto addMenuItem = [&](const char* label, bool alreadyExists, auto addComponent)
		{
			if (alreadyExists)
				return;
			hasAvailableComponent = true;
			if (ImGui::MenuItem(label))
			{
				addComponent();
				ImGui::CloseCurrentPopup();
			}
		};

		addMenuItem("Camera", entity.HasComponent<CameraComponent>(), [&]() { entity.AddComponent<CameraComponent>(); });
		addMenuItem("Sprite Renderer", entity.HasComponent<SpriteRendererComponent>(), [&]() { entity.AddComponent<SpriteRendererComponent>(); });
		addMenuItem("Model Renderer", entity.HasComponent<ModelRendererComponent>(), [&]() { entity.AddComponent<ModelRendererComponent>(); });
		addMenuItem("Material", entity.HasComponent<MaterialComponent>(), [&]() { entity.AddComponent<MaterialComponent>(); });
		addMenuItem("Terrain", entity.HasComponent<TerrainComponent>(), [&]() {
			auto& terrain = entity.AddComponent<TerrainComponent>();
			terrain.Specification.RenderShaderHandle = AssetManager::ImportAsset("assets/shaders/Terrain.glsl");
			terrain.Specification.GenerationShaderHandle = AssetManager::ImportAsset("assets/shaders/Terrain/GenerateFBM.comp");
		});
		if (hasAvailableComponent)
			ImGui::Separator();
		addMenuItem("Directional Light", entity.HasComponent<DirectionalLightComponent>(), [&]() { entity.AddComponent<DirectionalLightComponent>(); });
		addMenuItem("Point Light", entity.HasComponent<PointLightComponent>(), [&]() { entity.AddComponent<PointLightComponent>(); });
		addMenuItem("Sky Light", entity.HasComponent<SkyLightComponent>(), [&]() { entity.AddComponent<SkyLightComponent>(); });

		if (!hasAvailableComponent)
			ImGui::TextDisabled("No components available.");
		ImGui::EndPopup();
	}

}
