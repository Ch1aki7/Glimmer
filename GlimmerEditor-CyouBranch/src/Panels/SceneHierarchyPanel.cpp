#include "SceneHierarchyPanel.h"
#include "InspectorPanel.h"
#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/Material.h"
#include "Glimmer/Renderer/TerrainRenderer.h"
#include "Glimmer/Terrain/Terrain.h"
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace gl {
	namespace
	{
		bool SameMaterialProperties(
			const MaterialProperties& left,
			const MaterialProperties& right)
		{
			return glm::all(glm::equal(left.BaseColor, right.BaseColor))
				&& left.BaseColorTexture == right.BaseColorTexture
				&& left.NormalTexture == right.NormalTexture
				&& left.AOTexture == right.AOTexture
				&& left.EmissiveTexture == right.EmissiveTexture
				&& left.TilingFactor == right.TilingFactor
				&& left.Metallic == right.Metallic
				&& left.Roughness == right.Roughness
				&& left.NormalScale == right.NormalScale
				&& left.AOStrength == right.AOStrength
				&& glm::all(glm::equal(left.EmissiveColor, right.EmissiveColor))
				&& left.EmissiveStrength == right.EmissiveStrength
				&& left.AlphaMode == right.AlphaMode
				&& left.AlphaCutoff == right.AlphaCutoff;
		}

		bool SameMaterialComponent(
			const MaterialComponent& left,
			const MaterialComponent& right)
		{
			return left.MaterialHandle == right.MaterialHandle
				&& left.Overrides.Mask == right.Overrides.Mask
				&& SameMaterialProperties(
					left.Overrides.Values, right.Overrides.Values);
		}
	}

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
			Entity entity = m_Context->CreateEntity();
			const EntitySnapshot snapshot = EntitySnapshot::Capture(entity);
			SetSelectedEntity(entity);
			if (OnEntitySelected) OnEntitySelected(entity);

			if (m_CommandHistory)
			{
				const Ref<Scene> scene = m_Context;
				m_CommandHistory->PushExecuted(std::make_unique<LambdaEditorCommand>(
					"Create Entity",
					[this, scene, snapshot]() {
						Entity restored = snapshot.Restore(scene);
						SetSelectedEntity(restored);
						if (OnEntitySelected) OnEntitySelected(restored);
					},
					[this, scene, snapshot]() {
						Entity target = scene->FindEntityByUUID(snapshot.ID);
						if (target)
							scene->DestroyEntity(target);
						SetSelectedEntity({});
					}));
			}
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
				const EntitySnapshot snapshot =
					EntitySnapshot::Capture(m_RightClickedEntity);
				if (m_SelectionContext == m_RightClickedEntity)
					SetSelectedEntity({});

				if (OnEntityDeleted)
					OnEntityDeleted(m_RightClickedEntity);

				m_Context->DestroyEntity(m_RightClickedEntity);
				if (m_CommandHistory)
				{
					const Ref<Scene> scene = m_Context;
					m_CommandHistory->PushExecuted(std::make_unique<LambdaEditorCommand>(
						"Delete Entity",
						[this, scene, snapshot]() {
							Entity target = scene->FindEntityByUUID(snapshot.ID);
							if (target)
								scene->DestroyEntity(target);
							SetSelectedEntity({});
						},
						[this, scene, snapshot]() {
							Entity restored = snapshot.Restore(scene);
							SetSelectedEntity(restored);
							if (OnEntitySelected) OnEntitySelected(restored);
						}));
				}
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
				Entity duplicate = m_Context->DuplicateEntity(entity);
				const EntitySnapshot snapshot = EntitySnapshot::Capture(duplicate);
				SetSelectedEntity(duplicate);
				if (OnEntitySelected) OnEntitySelected(m_SelectionContext);
				if (m_CommandHistory)
				{
					const Ref<Scene> scene = m_Context;
					m_CommandHistory->PushExecuted(std::make_unique<LambdaEditorCommand>(
						"Duplicate Entity",
						[this, scene, snapshot]() {
							Entity restored = snapshot.Restore(scene);
							SetSelectedEntity(restored);
							if (OnEntitySelected) OnEntitySelected(restored);
						},
						[this, scene, snapshot]() {
							Entity target = scene->FindEntityByUUID(snapshot.ID);
							if (target)
								scene->DestroyEntity(target);
							SetSelectedEntity({});
						}));
				}
			}
			if (ImGui::MenuItem("Delete")) {
				m_RightClickedEntity = entity;
				m_ShowDeletePopup = true;
			}
			ImGui::EndPopup();
		}

		(void)idCounter;
	}

	void InspectorPanel::ExecuteMaterialComponentEdit(
		Entity entity,
		const char* name,
		const MaterialComponent& before,
		const MaterialComponent& after)
	{
		if (!entity || SameMaterialComponent(before, after))
			return;

		const Ref<Scene> scene = m_Context;
		const UUID uuid = entity.GetUUID();
		auto apply = [scene, uuid](const MaterialComponent& value) {
			Entity target = scene ? scene->FindEntityByUUID(uuid) : Entity{};
			if (!target || !target.HasComponent<MaterialComponent>())
				return false;
			target.GetComponent<MaterialComponent>() = value;
			return true;
		};

		if (m_CommandHistory)
		{
			m_CommandHistory->Execute(
				std::make_unique<ValueEditorCommand<MaterialComponent>>(
					name, before, after, apply));
		}
		else
		{
			apply(after);
		}
	}

	void InspectorPanel::CommitMaterialComponentWidget(
		Entity entity,
		const char* name,
		const MaterialComponent& after)
	{
		if (!m_MaterialComponentEdit.IsActive())
			return;

		const MaterialComponent before = m_MaterialComponentEdit.GetBefore();
		m_MaterialComponentEdit.Reset();
		if (!m_CommandHistory || !m_Context
			|| SameMaterialComponent(before, after))
			return;

		const Ref<Scene> scene = m_Context;
		const UUID uuid = entity.GetUUID();
		auto apply = [scene, uuid](const MaterialComponent& value) {
			Entity target = scene->FindEntityByUUID(uuid);
			if (!target || !target.HasComponent<MaterialComponent>())
				return false;
			target.GetComponent<MaterialComponent>() = value;
			return true;
		};
		m_CommandHistory->PushExecuted(
			std::make_unique<ValueEditorCommand<MaterialComponent>>(
				name, before, after, apply));
	}

	void InspectorPanel::DrawComponents(Entity entity)
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
						m_TransformEdit.Begin(valueBeforeWidget);

					if (ImGui::IsItemDeactivatedAfterEdit()
						&& m_TransformEdit.IsActive())
					{
						if (m_CommandHistory && m_Context)
						{
							const TransformComponent before = m_TransformEdit.GetBefore();
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
						}
						m_TransformEdit.Reset();
					}
				};

				drawVector("Position", transform.Translation, 0.1f);
				drawVector("Rotation", transform.Rotation, 1.0f);
				drawVector("Scale", transform.Scale, 0.05f, 0.01f, 10.0f);
			},
			false);

		// --- Terrain ---
		DrawComponent<TerrainComponent>("Terrain", entity,
			[this, entity](TerrainComponent& terrain) {
				auto& spec = terrain.Specification;
				auto commit = [this, entity, &terrain](
					const char* name, const TerrainComponent& before) {
					CommitComponentWidget(entity, name, m_TerrainEdit,
						before, terrain);
				};

				TerrainComponent before = terrain;
				if (ImGui::Checkbox("Procedural", &spec.Procedural))
					terrain.Runtime.reset();
				commit("Edit Terrain Procedural Mode", before);

				if (spec.Procedural)
				{
					const char* presetNames[] = {
						"Custom", "Alpine", "Plateau", "Rolling Hills",
						"Volcanic", "Eroded Valley" };
					int presetIndex = static_cast<int>(spec.Preset);
					if (ImGui::Combo("Preset", &presetIndex, presetNames,
						IM_ARRAYSIZE(presetNames)))
					{
						TerrainComponent after = terrain;
						ApplyTerrainPreset(after.Specification,
							static_cast<TerrainPreset>(presetIndex));
						ExecuteComponentEdit(entity, "Apply Terrain Preset",
							terrain, after);
					}
				}

				int heightResolution = static_cast<int>(spec.HeightMapResolution);
				before = terrain;
				if (ImGui::InputInt("Height Resolution", &heightResolution, 0))
				{
					spec.HeightMapResolution = static_cast<uint32_t>(std::clamp(heightResolution, 64, 2048));
					terrain.Runtime.reset();
				}
				commit("Edit Terrain Height Resolution", before);

				int meshResolution = static_cast<int>(spec.MeshResolution);
				before = terrain;
				if (ImGui::InputInt("Mesh Resolution", &meshResolution, 0))
				{
					spec.MeshResolution = static_cast<uint32_t>(std::clamp(meshResolution, 16, 512));
					terrain.Runtime.reset();
				}
				commit("Edit Terrain Mesh Resolution", before);

				before = terrain;
				if (ImGui::DragFloat("Height Scale", &spec.HeightScale, 0.1f, 0.0f, 500.0f))
				{
					spec.Preset = TerrainPreset::Custom;
					TerrainRenderer::Invalidate(terrain);
				}
				commit("Edit Terrain Height Scale", before);

				if (spec.Procedural)
				{
					auto& noise = spec.Noise;
					auto drawNoise = [this, entity, &terrain, &commit](
						const char* commandName, auto drawWidget) {
						const TerrainComponent valueBeforeWidget = terrain;
						if (drawWidget())
						{
							terrain.Specification.Preset = TerrainPreset::Custom;
							TerrainRenderer::Invalidate(terrain);
						}
						commit(commandName, valueBeforeWidget);
					};

					drawNoise("Edit Terrain Seed", [&]() {
						return ImGui::DragInt("Seed", &noise.Seed, 1.0f);
					});
					drawNoise("Edit Terrain Octaves", [&]() {
						return ImGui::SliderInt("Octaves", &noise.Octaves, 1, 12);
					});
					drawNoise("Edit Terrain Frequency", [&]() {
						return ImGui::DragFloat("Frequency", &noise.Frequency, 0.01f, 0.05f, 32.0f);
					});
					drawNoise("Edit Terrain Lacunarity", [&]() {
						return ImGui::DragFloat("Lacunarity", &noise.Lacunarity, 0.01f, 1.0f, 4.0f);
					});
					drawNoise("Edit Terrain Persistence", [&]() {
						return ImGui::DragFloat("Persistence", &noise.Persistence, 0.01f, 0.05f, 0.95f);
					});
					drawNoise("Edit Terrain Domain Warp", [&]() {
						return ImGui::DragFloat("Domain Warp", &noise.DomainWarp, 0.01f, 0.0f, 4.0f);
					});
					drawNoise("Edit Terrain Ridge Strength", [&]() {
						return ImGui::SliderFloat("Ridge Strength", &noise.RidgeStrength, 0.0f, 1.0f);
					});
					drawNoise("Edit Terrain Continent Scale", [&]() {
						return ImGui::SliderFloat("Continent Scale", &noise.ContinentScale, 0.05f, 1.0f);
					});
					drawNoise("Edit Terrain Channel Erosion", [&]() {
						return ImGui::SliderFloat("Channel Erosion", &noise.ErosionStrength, 0.0f, 0.5f);
					});
					drawNoise("Edit Terrain Detail Strength", [&]() {
						return ImGui::SliderFloat("Detail Strength", &noise.DetailStrength, 0.0f, 0.25f);
					});
					drawNoise("Edit Terrain Mountain Direction", [&]() {
						return ImGui::SliderAngle("Mountain Direction",
							&noise.MountainDirection, -180.0f, 180.0f);
					});
					drawNoise("Edit Terrain Mountain Width", [&]() {
						return ImGui::SliderFloat("Mountain Width",
							&noise.MountainWidth, 0.05f, 1.0f);
					});
					drawNoise("Edit Terrain Plateau Strength", [&]() {
						return ImGui::SliderFloat("Plateau Strength",
							&noise.PlateauStrength, 0.0f, 1.0f);
					});
					drawNoise("Edit Terrain Offset", [&]() {
						return ImGui::DragFloat2("Offset", &noise.Offset.x, 0.005f);
					});

					ImGui::SeparatorText("Authoring Erosion");
					auto& authoring = spec.Authoring;
					drawNoise("Edit Terrain Thermal Erosion Enabled", [&]() {
						return ImGui::Checkbox("Enable Thermal Erosion",
							&authoring.EnableThermalErosion);
					});
					int thermalIterations =
						static_cast<int>(authoring.ThermalIterations);
					drawNoise("Edit Terrain Thermal Iterations", [&]() {
						const bool changed = ImGui::InputInt(
							"Thermal Iterations", &thermalIterations);
						if (changed)
							authoring.ThermalIterations = static_cast<uint32_t>(
								std::clamp(thermalIterations, 0, 128));
						return changed;
					});
					drawNoise("Edit Terrain Talus", [&]() {
						return ImGui::DragFloat("Talus", &authoring.Talus,
							0.0005f, 0.0001f, 0.25f, "%.4f");
					});
					drawNoise("Edit Terrain Thermal Strength", [&]() {
						return ImGui::SliderFloat("Thermal Strength",
							&authoring.ThermalStrength, 0.0f, 0.5f);
					});
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
								const TerrainComponent beforeDrop = terrain;
								TerrainComponent afterDrop = terrain;
								afterDrop.Specification.HeightMapHandle = handle;
								ExecuteComponentEdit(entity,
									"Set Terrain Height Map", beforeDrop, afterDrop);
							}
						}
						ImGui::EndDragDropTarget();
					}
				}
				if (ImGui::Button("Regenerate"))
					TerrainRenderer::Invalidate(terrain);
				if (terrain.Runtime && terrain.Runtime->GenerationVersion > 0)
				{
					ImGui::TextDisabled("Generation v%llu | %u compute dispatches",
						static_cast<unsigned long long>(terrain.Runtime->GenerationVersion),
						terrain.Runtime->LastGenerationDispatchCount);
					ImGui::TextDisabled("Derived: Normal/Slope, Curvature/Flow, Weights");
				}
			},
			true, false);
		// --- Directional Light ---
		DrawComponent<DirectionalLightComponent>("Directional Light", entity,
			[this, entity](DirectionalLightComponent& light) {
				DirectionalLightComponent before = light;
				ImGui::Checkbox("Enabled##Directional", &light.Enabled);
				CommitComponentWidget(entity, "Edit Directional Light Enabled",
					m_DirectionalLightEdit, before, light);
				before = light;
				ImGui::ColorEdit3("Color##Directional", glm::value_ptr(light.Color));
				CommitComponentWidget(entity, "Edit Directional Light Color",
					m_DirectionalLightEdit, before, light);
				before = light;
				ImGui::DragFloat("Intensity##Directional", &light.Intensity,
					0.05f, 0.0f, 100.0f);
				CommitComponentWidget(entity, "Edit Directional Light Intensity",
					m_DirectionalLightEdit, before, light);
				before = light;
				ImGui::DragFloat("Ambient##Directional", &light.AmbientIntensity,
					0.01f, 0.0f, 10.0f);
				CommitComponentWidget(entity, "Edit Directional Light Ambient",
					m_DirectionalLightEdit, before, light);
				ImGui::TextDisabled("Direction follows Transform rotation.");
			});

		// --- Point Light ---
		DrawComponent<PointLightComponent>("Point Light", entity,
			[this, entity](PointLightComponent& light) {
				PointLightComponent before = light;
				ImGui::Checkbox("Enabled##Point", &light.Enabled);
				CommitComponentWidget(entity, "Edit Point Light Enabled",
					m_PointLightEdit, before, light);
				before = light;
				ImGui::ColorEdit3("Color##Point", glm::value_ptr(light.Color));
				CommitComponentWidget(entity, "Edit Point Light Color",
					m_PointLightEdit, before, light);
				before = light;
				ImGui::DragFloat("Intensity##Point", &light.Intensity,
					0.1f, 0.0f, 1000.0f);
				CommitComponentWidget(entity, "Edit Point Light Intensity",
					m_PointLightEdit, before, light);
				before = light;
				ImGui::DragFloat("Range##Point", &light.Range,
					0.1f, 0.01f, 1000.0f);
				CommitComponentWidget(entity, "Edit Point Light Range",
					m_PointLightEdit, before, light);
			});

		// --- Sky Light ---
		DrawComponent<SkyLightComponent>("Sky Light", entity,
			[this, entity](SkyLightComponent& skyLight) {
				const AssetMetadata metadata =
					AssetManager::GetMetadata(skyLight.CubemapHandle);
				const bool hasCubemap = metadata.IsValid()
					&& metadata.Type == AssetType::Cubemap;
				const std::string assetName = hasCubemap
					? metadata.FilePath.filename().string()
					: "None (drag .glsky here)";

				SkyLightComponent before = skyLight;
				ImGui::Checkbox("Enabled##SkyLight", &skyLight.Enabled);
				CommitComponentWidget(entity, "Edit Sky Light Enabled",
					m_SkyLightEdit, before, skyLight);
				before = skyLight;
				ImGui::DragFloat("Intensity##SkyLight", &skyLight.Intensity,
					0.05f, 0.0f, 20.0f);
				CommitComponentWidget(entity, "Edit Sky Light Intensity",
					m_SkyLightEdit, before, skyLight);
				ImGui::Text("Cubemap: %s", assetName.c_str());
				ImGui::SameLine();
				if (hasCubemap && ImGui::SmallButton("X##SkyLight"))
				{
					SkyLightComponent after = skyLight;
					after.CubemapHandle = AssetHandle(0);
					ExecuteComponentEdit(entity, "Clear Sky Light Cubemap",
						skyLight, after);
				}

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
						{
							SkyLightComponent after = skyLight;
							after.CubemapHandle = handle;
							ExecuteComponentEdit(entity, "Set Sky Light Cubemap",
								skyLight, after);
						}
					}
					ImGui::EndDragDropTarget();
				}
			});
		// --- Camera ---
		DrawComponent<CameraComponent>("Camera", entity,
			[this, entity](CameraComponent& cameraComponent) {
				auto& camera = cameraComponent.Camera;

				CameraComponent before = cameraComponent;
				ImGui::Checkbox("Primary", &cameraComponent.Primary);
				CommitComponentWidget(entity, "Edit Camera Primary",
					m_CameraEdit, before, cameraComponent);

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
							CameraComponent after = cameraComponent;
							after.Camera.SetProjectionType((SceneCamera::ProjectionType)i);
							ExecuteComponentEdit(entity, "Edit Camera Projection",
								cameraComponent, after);
						}
						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
				{
					float perspectiveVerticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
					before = cameraComponent;
					if (ImGui::DragFloat("Vertical FOV", &perspectiveVerticalFov, 0.5f, 1.0f, 179.0f))
						camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));
					CommitComponentWidget(entity, "Edit Camera Vertical FOV",
						m_CameraEdit, before, cameraComponent);

					float perspectiveNear = camera.GetPerspectiveNearClip();
					before = cameraComponent;
					if (ImGui::DragFloat("Near", &perspectiveNear, 0.01f, 0.001f))
						camera.SetPerspectiveNearClip(perspectiveNear);
					CommitComponentWidget(entity, "Edit Camera Near Clip",
						m_CameraEdit, before, cameraComponent);

					float perspectiveFar = camera.GetPerspectiveFarClip();
					before = cameraComponent;
					if (ImGui::DragFloat("Far", &perspectiveFar, 1.0f))
						camera.SetPerspectiveFarClip(perspectiveFar);
					CommitComponentWidget(entity, "Edit Camera Far Clip",
						m_CameraEdit, before, cameraComponent);
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
				{
					float orthoSize = camera.GetOrthographicSize();
					before = cameraComponent;
					if (ImGui::DragFloat("Size", &orthoSize, 0.1f, 0.1f))
						camera.SetOrthographicSize(orthoSize);
					CommitComponentWidget(entity, "Edit Camera Orthographic Size",
						m_CameraEdit, before, cameraComponent);

					float orthoNear = camera.GetOrthographicNearClip();
					before = cameraComponent;
					if (ImGui::DragFloat("Near", &orthoNear, 0.1f))
						camera.SetOrthographicNearClip(orthoNear);
					CommitComponentWidget(entity, "Edit Camera Near Clip",
						m_CameraEdit, before, cameraComponent);

					float orthoFar = camera.GetOrthographicFarClip();
					before = cameraComponent;
					if (ImGui::DragFloat("Far", &orthoFar, 0.1f))
						camera.SetOrthographicFarClip(orthoFar);
					CommitComponentWidget(entity, "Edit Camera Far Clip",
						m_CameraEdit, before, cameraComponent);

					before = cameraComponent;
					ImGui::Checkbox("Fixed Aspect Ratio", &cameraComponent.FixedAspectRatio);
					CommitComponentWidget(entity, "Edit Camera Fixed Aspect Ratio",
						m_CameraEdit, before, cameraComponent);
				}
			});

		// --- Model Renderer ---
		DrawComponent<ModelRendererComponent>("Model Renderer", entity,
			[](ModelRendererComponent& component) {
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
			});

		// --- Sprite Renderer ---
		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity,
			[](SpriteRendererComponent& src) {
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
			});
		// --- Material ---
		DrawComponent<MaterialComponent>("Material", entity,
			[this, entity](MaterialComponent& component) {
				const AssetMetadata metadata = AssetManager::GetMetadata(component.MaterialHandle);
				const bool hasMaterial = metadata.IsValid()
					&& metadata.Type == AssetType::Material;

				const std::string materialName = hasMaterial
					? metadata.FilePath.filename().string()
					: "None (drag .glmat here)";
				ImGui::Text("Asset: %s", materialName.c_str());
				ImGui::SameLine();
				if (hasMaterial && ImGui::SmallButton("X##Material"))
				{
					const MaterialComponent before = component;
					MaterialComponent after = before;
					after.MaterialHandle = AssetHandle(0);
					after.Overrides.Clear();
					ExecuteMaterialComponentEdit(
						entity, "Clear Entity Material", before, after);
				}

				if (ImGui::BeginDragDropTarget())
				{
					if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
					{
						std::string path((const char*)payload->Data, payload->DataSize - 1);
						std::string extension = std::filesystem::path(path).extension().string();
						std::transform(extension.begin(), extension.end(), extension.begin(),
							[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
						if (extension == ".glmat")
						{
							const AssetHandle handle = AssetManager::ImportAsset(path);
							if (AssetManager::GetMetadata(handle).Type == AssetType::Material
								&& handle != component.MaterialHandle)
							{
								const MaterialComponent before = component;
								MaterialComponent after = before;
								after.MaterialHandle = handle;
								after.Overrides.Clear();
								ExecuteMaterialComponentEdit(
									entity, "Set Entity Material", before, after);
							}
						}
					}
					ImGui::EndDragDropTarget();
				}

				if (Ref<Material> material = AssetManager::GetMaterial(component.MaterialHandle))
				{
					const auto& base = material->GetProperties();
					auto& overrides = component.Overrides;
					auto& values = overrides.Values;

					const AssetMetadata shaderMetadata =
						AssetManager::GetMetadata(material->GetShaderHandle());
					const bool hasShader = shaderMetadata.IsValid()
						&& shaderMetadata.Type == AssetType::Shader;
					const std::string shaderName = hasShader
						? shaderMetadata.FilePath.filename().string()
						: "None";
					ImGui::Text("Shader (inherited): %s", shaderName.c_str());
					ImGui::TextDisabled("Checkboxes enable per-entity overrides.");

					auto toggleOverride = [this, entity, &component](
						const char* id, MaterialOverride property, auto initialize) {
						bool enabled = component.Overrides.IsEnabled(property);
						if (ImGui::Checkbox(id, &enabled))
						{
							const MaterialComponent before = component;
							MaterialComponent after = before;
							if (enabled)
								initialize(after.Overrides.Values);
							after.Overrides.SetEnabled(property, enabled);
							ExecuteMaterialComponentEdit(
								entity, "Toggle Material Override", before, after);
						}
						return component.Overrides.IsEnabled(property);
					};
					auto trackContinuousEdit = [this, entity, &component](
						const char* name, const MaterialComponent& beforeWidget) {
						if (ImGui::IsItemActivated())
							m_MaterialComponentEdit.Begin(beforeWidget);
						if (ImGui::IsItemDeactivatedAfterEdit())
							CommitMaterialComponentWidget(entity, name, component);
					};

					bool overrideAlphaMode = toggleOverride(
						"##OverrideAlphaMode", MaterialOverride::AlphaMode,
						[&](MaterialProperties& target) { target.AlphaMode = base.AlphaMode; });
					ImGui::SameLine();
					static const char* alphaModes[] = { "Opaque", "Mask", "Blend" };
					int alphaMode = static_cast<int>(overrideAlphaMode
						? values.AlphaMode : base.AlphaMode);
					ImGui::BeginDisabled(!overrideAlphaMode);
					if (ImGui::Combo("Alpha Mode", &alphaMode, alphaModes, 3)
						&& overrideAlphaMode)
					{
						const MaterialComponent before = component;
						MaterialComponent after = before;
						after.Overrides.Values.AlphaMode =
							static_cast<MaterialAlphaMode>(alphaMode);
						after.Overrides.MarkDirty();
						ExecuteMaterialComponentEdit(
							entity, "Set Alpha Mode Override", before, after);
					}
					ImGui::EndDisabled();

					bool overrideAlphaCutoff = toggleOverride(
						"##OverrideAlphaCutoff", MaterialOverride::AlphaCutoff,
						[&](MaterialProperties& target) { target.AlphaCutoff = base.AlphaCutoff; });
					ImGui::SameLine();
					float inheritedAlphaCutoff = base.AlphaCutoff;
					ImGui::BeginDisabled(!overrideAlphaCutoff);
					MaterialComponent beforeWidget = component;
					if (ImGui::SliderFloat("Alpha Cutoff",
						overrideAlphaCutoff ? &values.AlphaCutoff : &inheritedAlphaCutoff,
						0.0f, 1.0f) && overrideAlphaCutoff)
						overrides.MarkDirty();
					if (overrideAlphaCutoff)
						trackContinuousEdit(
							"Edit Alpha Cutoff Override", beforeWidget);
					ImGui::EndDisabled();

					bool overrideBaseColor = toggleOverride(
						"##OverrideBaseColor", MaterialOverride::BaseColor,
						[&](MaterialProperties& target) { target.BaseColor = base.BaseColor; });
					ImGui::SameLine();
					glm::vec4 inheritedBaseColor = base.BaseColor;
					ImGui::BeginDisabled(!overrideBaseColor);
					beforeWidget = component;
					if (ImGui::ColorEdit4("Base Color",
						glm::value_ptr(overrideBaseColor ? values.BaseColor : inheritedBaseColor))
						&& overrideBaseColor)
						overrides.MarkDirty();
					if (overrideBaseColor)
						trackContinuousEdit(
							"Edit Base Color Override", beforeWidget);
					ImGui::EndDisabled();

					bool overrideTiling = toggleOverride(
						"##OverrideTiling", MaterialOverride::TilingFactor,
						[&](MaterialProperties& target) { target.TilingFactor = base.TilingFactor; });
					ImGui::SameLine();
					float inheritedTiling = base.TilingFactor;
					ImGui::BeginDisabled(!overrideTiling);
					beforeWidget = component;
					if (ImGui::DragFloat("Material Tiling",
						overrideTiling ? &values.TilingFactor : &inheritedTiling,
						0.05f, 0.01f, 100.0f) && overrideTiling)
						overrides.MarkDirty();
					if (overrideTiling)
						trackContinuousEdit(
							"Edit Tiling Override", beforeWidget);
					ImGui::EndDisabled();

					bool overrideMetallic = toggleOverride(
						"##OverrideMetallic", MaterialOverride::Metallic,
						[&](MaterialProperties& target) { target.Metallic = base.Metallic; });
					ImGui::SameLine();
					float inheritedMetallic = base.Metallic;
					ImGui::BeginDisabled(!overrideMetallic);
					beforeWidget = component;
					if (ImGui::SliderFloat("Metallic",
						overrideMetallic ? &values.Metallic : &inheritedMetallic,
						0.0f, 1.0f) && overrideMetallic)
						overrides.MarkDirty();
					if (overrideMetallic)
						trackContinuousEdit(
							"Edit Metallic Override", beforeWidget);
					ImGui::EndDisabled();

					bool overrideRoughness = toggleOverride(
						"##OverrideRoughness", MaterialOverride::Roughness,
						[&](MaterialProperties& target) { target.Roughness = base.Roughness; });
					ImGui::SameLine();
					float inheritedRoughness = base.Roughness;
					ImGui::BeginDisabled(!overrideRoughness);
					beforeWidget = component;
					if (ImGui::SliderFloat("Roughness",
						overrideRoughness ? &values.Roughness : &inheritedRoughness,
						0.04f, 1.0f) && overrideRoughness)
						overrides.MarkDirty();
					if (overrideRoughness)
						trackContinuousEdit(
							"Edit Roughness Override", beforeWidget);
					ImGui::EndDisabled();

					bool overrideNormalScale = toggleOverride(
						"##OverrideNormalScale", MaterialOverride::NormalScale,
						[&](MaterialProperties& target) { target.NormalScale = base.NormalScale; });
					ImGui::SameLine();
					float inheritedNormalScale = base.NormalScale;
					ImGui::BeginDisabled(!overrideNormalScale);
					beforeWidget = component;
					if (ImGui::SliderFloat("Normal Scale",
						overrideNormalScale ? &values.NormalScale : &inheritedNormalScale,
						0.0f, 2.0f) && overrideNormalScale)
						overrides.MarkDirty();
					if (overrideNormalScale)
						trackContinuousEdit("Edit Normal Scale Override", beforeWidget);
					ImGui::EndDisabled();

					bool overrideAOStrength = toggleOverride(
						"##OverrideAOStrength", MaterialOverride::AOStrength,
						[&](MaterialProperties& target) { target.AOStrength = base.AOStrength; });
					ImGui::SameLine();
					float inheritedAOStrength = base.AOStrength;
					ImGui::BeginDisabled(!overrideAOStrength);
					beforeWidget = component;
					if (ImGui::SliderFloat("AO Strength",
						overrideAOStrength ? &values.AOStrength : &inheritedAOStrength,
						0.0f, 1.0f) && overrideAOStrength)
						overrides.MarkDirty();
					if (overrideAOStrength)
						trackContinuousEdit("Edit AO Strength Override", beforeWidget);
					ImGui::EndDisabled();

					bool overrideEmissiveColor = toggleOverride(
						"##OverrideEmissiveColor", MaterialOverride::EmissiveColor,
						[&](MaterialProperties& target) { target.EmissiveColor = base.EmissiveColor; });
					ImGui::SameLine();
					glm::vec3 inheritedEmissiveColor = base.EmissiveColor;
					ImGui::BeginDisabled(!overrideEmissiveColor);
					beforeWidget = component;
					if (ImGui::ColorEdit3("Emissive Color", glm::value_ptr(
						overrideEmissiveColor ? values.EmissiveColor : inheritedEmissiveColor))
						&& overrideEmissiveColor)
						overrides.MarkDirty();
					if (overrideEmissiveColor)
						trackContinuousEdit("Edit Emissive Color Override", beforeWidget);
					ImGui::EndDisabled();

					bool overrideEmissiveStrength = toggleOverride(
						"##OverrideEmissiveStrength", MaterialOverride::EmissiveStrength,
						[&](MaterialProperties& target) { target.EmissiveStrength = base.EmissiveStrength; });
					ImGui::SameLine();
					float inheritedEmissiveStrength = base.EmissiveStrength;
					ImGui::BeginDisabled(!overrideEmissiveStrength);
					beforeWidget = component;
					if (ImGui::DragFloat("Emissive Strength",
						overrideEmissiveStrength ? &values.EmissiveStrength : &inheritedEmissiveStrength,
						0.05f, 0.0f, 100.0f) && overrideEmissiveStrength)
						overrides.MarkDirty();
					if (overrideEmissiveStrength)
						trackContinuousEdit("Edit Emissive Strength Override", beforeWidget);
					ImGui::EndDisabled();

					auto drawTextureOverride = [&](const char* label, const char* id,
						MaterialOverride property, AssetHandle MaterialProperties::* field,
						TextureColorSpace colorSpace, TextureSemantic semantic) {
						bool enabled = toggleOverride(id, property,
							[&](MaterialProperties& target) { target.*field = base.*field; });
						ImGui::SameLine();
						const AssetHandle effective = enabled ? values.*field : base.*field;
						const AssetMetadata metadata = AssetManager::GetMetadata(effective);
						const bool hasTexture = metadata.IsValid()
							&& metadata.Type == AssetType::Texture2D;
						const std::string name = hasTexture
							? metadata.FilePath.filename().string() : "None";
						ImGui::PushID(label);
						ImGui::Text("%s: %s%s", label, name.c_str(), enabled ? "" : " (Inherited)");
						ImGui::SameLine();
						if (enabled && hasTexture && ImGui::SmallButton("X"))
						{
							const MaterialComponent before = component;
							MaterialComponent after = before;
							after.Overrides.Values.*field = AssetHandle(0);
							after.Overrides.MarkDirty();
							ExecuteMaterialComponentEdit(entity, "Clear Texture Override", before, after);
						}
						if (ImGui::BeginDragDropTarget())
						{
							if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
							{
								std::string path((const char*)payload->Data, payload->DataSize - 1);
								const AssetHandle textureHandle = AssetManager::ImportAsset(path);
								if (AssetManager::GetMetadata(textureHandle).Type == AssetType::Texture2D)
								{
									AssetManager::SetTextureMetadata(textureHandle, colorSpace, semantic);
									const MaterialComponent before = component;
									MaterialComponent after = before;
									after.Overrides.Values.*field = textureHandle;
									after.Overrides.MarkDirty();
									after.Overrides.SetEnabled(property, true);
									ExecuteMaterialComponentEdit(entity, "Set Texture Override", before, after);
								}
							}
							ImGui::EndDragDropTarget();
						}
						ImGui::PopID();
					};
					drawTextureOverride("Base Color Texture", "##OverrideBaseColorTexture",
						MaterialOverride::BaseColorTexture, &MaterialProperties::BaseColorTexture,
						TextureColorSpace::SRGB, TextureSemantic::Color);
					drawTextureOverride("Normal Texture", "##OverrideNormalTexture",
						MaterialOverride::NormalTexture, &MaterialProperties::NormalTexture,
						TextureColorSpace::Linear, TextureSemantic::Normal);
					drawTextureOverride("AO Texture", "##OverrideAOTexture",
						MaterialOverride::AOTexture, &MaterialProperties::AOTexture,
						TextureColorSpace::Linear, TextureSemantic::Data);
					drawTextureOverride("Emissive Texture", "##OverrideEmissiveTexture",
						MaterialOverride::EmissiveTexture, &MaterialProperties::EmissiveTexture,
						TextureColorSpace::SRGB, TextureSemantic::Color);

					if (!overrides.Empty() && ImGui::Button("Reset Overrides"))
					{
						const MaterialComponent before = component;
						MaterialComponent after = before;
						after.Overrides.Clear();
						ExecuteMaterialComponentEdit(
							entity, "Reset Material Overrides", before, after);
					}
				}
			});
		ImGui::Spacing();
		ImGui::Separator();
		const float buttonWidth = std::min(220.0f, ImGui::GetContentRegionAvail().x);
		if (ImGui::Button("Add Component", ImVec2(buttonWidth, 0.0f)))
			ImGui::OpenPopup("AddComponentPopup");
		DrawAddComponentMenu(entity);
	}

	void InspectorPanel::DrawAddComponentMenu(Entity entity)
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

		addMenuItem("Camera", entity.HasComponent<CameraComponent>(), [&]() { AddComponent<CameraComponent>(entity, "Camera"); });
		addMenuItem("Sprite Renderer", entity.HasComponent<SpriteRendererComponent>(), [&]() { AddComponent<SpriteRendererComponent>(entity, "Sprite Renderer"); });
		addMenuItem("Model Renderer", entity.HasComponent<ModelRendererComponent>(), [&]() { AddComponent<ModelRendererComponent>(entity, "Model Renderer"); });
		addMenuItem("Material", entity.HasComponent<MaterialComponent>(), [&]() { AddComponent<MaterialComponent>(entity, "Material"); });
		addMenuItem("Terrain", entity.HasComponent<TerrainComponent>(), [&]() {
			TerrainComponent terrain;
			ApplyTerrainPreset(terrain.Specification, TerrainPreset::Alpine);
			terrain.Specification.RenderShaderHandle = AssetManager::ImportAsset("assets/shaders/Terrain.glsl");
			terrain.Specification.GenerationShaderHandle = AssetManager::ImportAsset("assets/shaders/Terrain/GenerateFBM.comp");
			terrain.Specification.ErosionShaderHandle = AssetManager::ImportAsset("assets/shaders/Terrain/ThermalErosion.comp");
			terrain.Specification.DerivationShaderHandle = AssetManager::ImportAsset("assets/shaders/Terrain/DeriveTerrainMaps.comp");
			AddComponent<TerrainComponent>(entity, "Terrain", terrain);
		});
		if (hasAvailableComponent)
			ImGui::Separator();
		addMenuItem("Directional Light", entity.HasComponent<DirectionalLightComponent>(), [&]() { AddComponent<DirectionalLightComponent>(entity, "Directional Light"); });
		addMenuItem("Point Light", entity.HasComponent<PointLightComponent>(), [&]() { AddComponent<PointLightComponent>(entity, "Point Light"); });
		addMenuItem("Sky Light", entity.HasComponent<SkyLightComponent>(), [&]() { AddComponent<SkyLightComponent>(entity, "Sky Light"); });

		if (!hasAvailableComponent)
			ImGui::TextDisabled("No components available.");
		ImGui::EndPopup();
	}

}
