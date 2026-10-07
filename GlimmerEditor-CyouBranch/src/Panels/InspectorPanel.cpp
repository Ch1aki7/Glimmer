#include "InspectorPanel.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/Material.h"
#include "Glimmer/Renderer/Cubemap.h"
#include "Glimmer/Terrain/TerrainMaterial.h"
#include "Glimmer/Terrain/Terrain.h"
#include "Glimmer/Renderer/TerrainRenderer.h"
#include "../Editor/TerrainRecipeEditor.h"

#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cfloat>

namespace gl
{
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

		bool SameMaterialState(const MaterialState& left, const MaterialState& right)
		{
			return left.ShaderHandle == right.ShaderHandle
				&& SameMaterialProperties(left.Properties, right.Properties)
				&& left.Passes == right.Passes;
		}
	}

	void InspectorPanel::ResetTerrainDiagnostics()
	{
		m_TerrainSnapshot = {}; m_TerrainCaptureStatus = TerrainQueryStatus::NotReady;
		m_TerrainDiagnosticEntity = m_TerrainDiagnosticIdentity = 0;
		m_TerrainQueryXZ = {}; m_TerrainClippedCount.reset(); m_TerrainClipVersion = {};
	}

	void InspectorPanel::DrawTerrainDiagnostics(Entity entity, TerrainComponent& terrain)
	{
		const auto version = TerrainRenderer::GetSurfaceVersion(terrain);
		const uint64_t entityID = entity.GetUUID();
		if (m_TerrainDiagnosticEntity != entityID || m_TerrainDiagnosticIdentity != version.Identity)
		{
			ResetTerrainDiagnostics();
			m_TerrainDiagnosticEntity = entityID; m_TerrainDiagnosticIdentity = version.Identity;
		}
		auto statusName = [](TerrainQueryStatus status) {
			switch (status) {
			case TerrainQueryStatus::Ready: return "Ready";
			case TerrainQueryStatus::InvalidData: return "InvalidData";
			case TerrainQueryStatus::InvalidPosition: return "InvalidPosition";
			case TerrainQueryStatus::OutOfBounds: return "OutOfBounds";
			case TerrainQueryStatus::StaleVersion: return "StaleVersion - capture again";
			case TerrainQueryStatus::UnsupportedSurface: return "UnsupportedSurface - procedural Data v2 required";
			default: return "NotReady";
			}
		};
		ImGui::SeparatorText("Terrain Authoring Diagnostics");
		int view = int(TerrainRenderer::GetAuthoringVisualizationMode());
		if (ImGui::Combo("Authoring View", &view, "None\0Protection\0Clipping\0"))
			TerrainRenderer::SetAuthoringVisualizationMode(TerrainRenderer::AuthoringVisualizationMode(view));
		ImGui::TextWrapped("Global view: Protection dark=0, cyan=0.5, yellow=1; Clipping magenta=clipped. Water is hidden in these views.");
		ImGui::TextWrapped("Static authoring data only; runtime erosion does not consume protection or update the snapshot.");
		ImGui::Text("Published identity / generation: %llu / %llu", (unsigned long long)version.Identity, (unsigned long long)version.Generation);
		ImGui::Text("Protection map: %s", terrain.Runtime && terrain.Runtime->ProtectionMap ? "published" : "absent (zero)");
		if (terrain.Runtime) {
			const auto resources = terrain.Runtime->GetResourceUsage();
			constexpr double mib = 1024.0 * 1024.0;
			ImGui::Text("Terrain resources: %u textures | %.3f MiB", resources.TextureCount, resources.TextureBytes() / mib);
			ImGui::Text("Generator / pending: %.3f / %.3f MiB", resources.GeneratorBytes / mib, resources.PendingGeneratorBytes / mib);
			ImGui::Text("Hydrology / climate: %.3f / %.3f MiB", resources.HydrologyBytes / mib, resources.ClimateBytes / mib);
			ImGui::Text("Other referenced / meshes: %.3f / %.3f MiB", resources.OtherReferencedTextureBytes / mib, resources.MeshBytes / mib);
			ImGui::Text("Initial height CPU: %.3f MiB", resources.InitialHeightCPUBytes / mib);
			ImGui::TextDisabled("Logical resident storage; shared assets are referenced bytes, not exclusive driver VRAM.");
			const auto& preparation = terrain.Runtime->Preparation;
			ImGui::Text("Last Prepare texture peak / lifetime: %.3f / %.3f MiB", preparation.Textures.PeakBytes / mib, preparation.LifetimePeakTextureBytes / mib);
			ImGui::Text("Last texture creates / releases: %llu / %llu | total creates: %llu",
				(unsigned long long)preparation.Textures.Allocations, (unsigned long long)preparation.Textures.Releases, (unsigned long long)preparation.TotalTextureAllocations);
			ImGui::Text("CPU Prepare / last Generate: %.3f / %.3f ms", preparation.CPUPrepareMilliseconds, preparation.LastCPUGenerationMilliseconds);
			ImGui::Text("CPU Environment / derived subset: %.3f / %.3f ms", preparation.CPUEnvironmentMilliseconds, preparation.CPUDerivedMilliseconds);
			ImGui::TextDisabled("CPU wall time; Generate includes static derivation, Environment includes runtime derivation. Do not add subsets.");
		}
		if (ImGui::Button("Capture Static Snapshot"))
			m_TerrainCaptureStatus = TerrainRenderer::CaptureSurfaceSnapshot(terrain, m_TerrainSnapshot);
		ImGui::TextWrapped("Last capture: %s", statusName(m_TerrainCaptureStatus));
		const auto captured = m_TerrainSnapshot.GetVersion();
		ImGui::Text("Snapshot identity / generation: %llu / %llu", (unsigned long long)captured.Identity, (unsigned long long)captured.Generation);
		if (m_TerrainSnapshot.GetWidth())
			ImGui::Text("%u x %u | World %.2f | Height %.2f", m_TerrainSnapshot.GetWidth(), m_TerrainSnapshot.GetHeight(), m_TerrainSnapshot.GetWorldSize(), m_TerrainSnapshot.GetHeightScale());
		ImGui::InputFloat2("Query Local XZ", glm::value_ptr(m_TerrainQueryXZ));
		const auto sample = m_TerrainSnapshot.Query(m_TerrainQueryXZ, version);
		ImGui::TextWrapped("Query: %s", statusName(sample.Status));
		if (sample.Status == TerrainQueryStatus::Ready)
		{
			ImGui::Text("Local height: %.4f | Slope: %.4f degrees", sample.Height, sample.SlopeDegrees);
			ImGui::Text("Local normal: %.4f, %.4f, %.4f", sample.Normal.x, sample.Normal.y, sample.Normal.z);
		}
		const bool canRead = version.Generation && terrain.Runtime && terrain.Runtime->PublishedSpecification.Procedural && terrain.Runtime->Generator;
		ImGui::BeginDisabled(!canRead);
		if (ImGui::Button("Read Clipping Count"))
		{
			m_TerrainClippedCount = terrain.Runtime->Generator->ReadRecipeClippedNodeCount();
			m_TerrainClipVersion = version;
		}
		ImGui::EndDisabled();
		if (m_TerrainClippedCount)
			ImGui::Text("Clipped nodes: %llu (%s)", (unsigned long long)*m_TerrainClippedCount, m_TerrainClipVersion == version ? "current" : "stale - read again");
		ImGui::TextDisabled("Capture / count buttons synchronously read GPU data; queries reuse CPU values.");
	}

    void InspectorPanel::DrawWaterSurface(Entity entity, TerrainComponent& terrain)
    {
        if(m_WaterEditEntity!=uint64_t(entity.GetUUID())) {m_WaterEdit.Reset();m_WaterEditEntity=entity.GetUUID();m_WaterTextureError.clear();}
        if(!ImGui::TreeNodeEx("Water Surface Appearance",ImGuiTreeNodeFlags_DefaultOpen)) {m_WaterEdit.Reset();return;}
        auto& water=terrain.Specification.Water;
        ImGui::TextWrapped("Saved per terrain. Requires Simulation and water depth. Appearance edits preserve simulation.");
        const auto scene=m_Context; const UUID uuid=entity.GetUUID();
        auto apply=[scene,uuid](const WaterSurfaceAppearance& value) {
            Entity target=scene?scene->FindEntityByUUID(uuid):Entity{};
            if(!target||!target.HasComponent<TerrainComponent>()) return false;
            target.GetComponent<TerrainComponent>().Specification.Water=value;return true;
        };
        auto discrete=[&](const char* label,auto widget) {
            const auto before=water;
            if(widget()&&water!=before&&m_CommandHistory)
                m_CommandHistory->PushExecuted(std::make_unique<ValueEditorCommand<WaterSurfaceAppearance>>(label,before,water,apply));
        };
        auto continuous=[&](const char* label,float& value,float lo,float hi) {
            const auto before=water;
            ImGui::SliderFloat(label,&value,lo,hi,"%.3f",ImGuiSliderFlags_AlwaysClamp);
            if(ImGui::IsItemActivated()) m_WaterEdit.Begin(before);
            if(ImGui::IsItemDeactivatedAfterEdit()&&m_WaterEdit.IsActive()) {
                auto initial=m_WaterEdit.GetBefore();m_WaterEdit.Reset();
                if(m_CommandHistory&&initial!=water)
                    m_CommandHistory->PushExecuted(std::make_unique<ValueEditorCommand<WaterSurfaceAppearance>>(label,initial,water,apply));
            }
        };
        discrete("Toggle Water Appearance",[&](){return ImGui::Checkbox("Water Enabled",&water.Enabled);});
        int quality=int(water.MeshQuality);
        discrete("Edit Water Mesh Quality",[&](){if(!ImGui::Combo("Water Mesh Quality",&quality,"Low (32)\0Balanced (64)\0High (128)\0")) return false;water.MeshQuality=uint32_t(quality);return true;});
        continuous("Water Normal Strength",water.NormalStrength,0,2);
        continuous("Water Wave Length",water.WaveLength,0.25f,256);
        continuous("Water Flow Strength",water.FlowStrength,0,2);
        continuous("Water Surface Roughness",water.Roughness,0.04f,1);
        continuous("Water Depth Absorption",water.Absorption,0,10);
        continuous("Water Refraction",water.RefractionPixels,0,32);
        continuous("Water Sediment Color",water.SedimentTint,0,4);
        continuous("Water Flow Foam",water.FoamStrength,0,1);
        continuous("Water Shore Foam",water.ShoreFoam,0,1);
        continuous("Water Shore Width",water.ShoreWidth,0.005f,4);
        continuous("Water Shore Wetness",water.ShoreWetness,0,1);
        discrete("Edit Water Normal Convention",[&](){return ImGui::Checkbox("Water Normal DirectX",&water.NormalDirectX);});
        auto texture=[&](const char* label,AssetHandle& handle,TextureSemantic semantic) {
            ImGui::PushID(label);
            const auto meta=AssetManager::GetMetadata(handle);
            ImGui::Text("%s: %s",label,uint64_t(handle)==0?"Built-in (periodic)":meta.IsValid()?meta.FilePath.filename().string().c_str():"Missing asset");
            if(ImGui::BeginDragDropTarget()) {
                if(const auto* payload=ImGui::AcceptDragDropPayload("SCENE_FILE")) {
                    if(payload->DataSize>1) {
                        const std::string path(static_cast<const char*>(payload->Data),payload->DataSize-1);
                        const auto candidate=AssetManager::ImportAsset(path);
                        const auto candidateMeta=AssetManager::GetMetadata(candidate);
                        if(candidateMeta.Type==AssetType::Texture2D&&candidateMeta.ColorSpace==TextureColorSpace::Linear&&candidateMeta.Semantic==semantic)
                        {
                            discrete("Set Water Texture",[&](){handle=candidate;return true;});m_WaterTextureError.clear();
                        } else m_WaterTextureError=semantic==TextureSemantic::Normal?"Wave Normal requires a Linear/Normal texture asset.":"Foam Noise requires a Linear/Data texture asset.";
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if(uint64_t(handle)!=0) discrete("Clear Water Texture",[&](){if(!ImGui::SmallButton("Use Built-in")) return false;handle=AssetHandle(0);return true;});
            ImGui::PopID();
        };
        texture("Wave Normal",water.NormalTexture,TextureSemantic::Normal);
        texture("Foam Noise",water.FoamTexture,TextureSemantic::Data);
        ImGui::TextDisabled("Drag Linear/Normal or Linear/Data textures from Content Browser.");
        if(!m_WaterTextureError.empty()) ImGui::TextWrapped("Texture rejected: %s",m_WaterTextureError.c_str());
        const auto error=ValidateWaterSurfaceAppearance(water);
        if(!error.empty()) ImGui::TextWrapped("Water rejected: %s",error.c_str());
        ImGui::TreePop();
    }

	void InspectorPanel::DrawTerrainRecipe(Entity entity, TerrainComponent& terrain)
	{
		ImGui::SeparatorText("Terrain Stamps");
		auto& spec = terrain.Specification;
		ImGui::TextDisabled("Ordered local X/Z operations | %zu / 64", spec.Recipe.Stamps.size());
		ImGui::TextWrapped("Set Height uses absolute local Y; Add uses a height delta. Runtime erosion can change this surface.");
		const auto validation = ValidateTerrainRecipe(spec.Recipe, spec.HeightScale, spec.DataVersion, spec.Procedural);
		if (!validation.Valid()) ImGui::TextWrapped("Recipe rejected: %s", validation.Message.c_str());
		if (terrain.Runtime && !terrain.Runtime->GenerationError.empty())
			ImGui::TextWrapped("Generation failed: %s", terrain.Runtime->GenerationError.c_str());
		const bool canAdd = spec.Procedural && spec.DataVersion == 2 && std::isfinite(spec.HeightScale)
			&& spec.HeightScale > 0 && spec.Recipe.Stamps.size() < 64 && validation.Valid();
		if (!canAdd) ImGui::TextDisabled("Adding requires procedural Data v2, positive Height Scale and fewer than 64 stamps.");
		ImGui::BeginDisabled(!canAdd);
		if (ImGui::Button("Add Rectangle"))
		{
			TerrainComponent after = terrain;
			if (TerrainRecipeEditor::Add(after.Specification, TerrainStampShape::Rectangle, TerrainStampOperation::SetHeight))
				ExecuteComponentEdit(entity, "Add Terrain Rectangle", terrain, after);
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Ellipse"))
		{
			TerrainComponent after = terrain;
			if (TerrainRecipeEditor::Add(after.Specification, TerrainStampShape::Ellipse, TerrainStampOperation::Add))
				ExecuteComponentEdit(entity, "Add Terrain Ellipse", terrain, after);
		}
		ImGui::EndDisabled();
		ImGui::BeginDisabled(spec.Recipe.Stamps.empty());
		if (ImGui::Button("Clear Stamps"))
		{
			TerrainComponent after = terrain;
			after.Specification.Recipe.Stamps.clear();
			ExecuteComponentEdit(entity, "Clear Terrain Stamps", terrain, after);
		}
		ImGui::EndDisabled();
		for (size_t index = 0; index < spec.Recipe.Stamps.size(); ++index)
		{
			auto& stamp = spec.Recipe.Stamps[index];
			// Both halves identify the widget independently of its position in the list.
			ImGui::PushID(int(stamp.ID >> 32));
			ImGui::PushID(int(stamp.ID & 0xffffffffu));
			const bool open = ImGui::TreeNodeEx("Stamp", ImGuiTreeNodeFlags_DefaultOpen,
				"%zu: %s / %s", index + 1, stamp.Shape == TerrainStampShape::Rectangle ? "Rectangle" : "Ellipse",
				stamp.Operation == TerrainStampOperation::SetHeight ? "Set Height" : "Add");
			if (open)
			{
				ImGui::TextDisabled("ID: %llu", static_cast<unsigned long long>(stamp.ID));
				int action = 0;
				ImGui::BeginDisabled(index == 0);
				if (ImGui::SmallButton("Up")) action = -1;
				ImGui::EndDisabled(); ImGui::SameLine();
				ImGui::BeginDisabled(index + 1 == spec.Recipe.Stamps.size());
				if (ImGui::SmallButton("Down")) action = 1;
				ImGui::EndDisabled(); ImGui::SameLine();
				if (ImGui::SmallButton("Remove")) action = 2;
				if (action)
				{
					TerrainComponent after = terrain;
					if (action == 2) TerrainRecipeEditor::Remove(after.Specification.Recipe, index);
					else TerrainRecipeEditor::Move(after.Specification.Recipe, index, action);
					ExecuteComponentEdit(entity, action == 2 ? "Remove Terrain Stamp" : "Reorder Terrain Stamps", terrain, after);
					ImGui::TreePop(); ImGui::PopID(); ImGui::PopID();
					break;
				}
				auto discrete = [&](const char* name, auto widget) {
					const TerrainComponent before = terrain;
					if (widget())
					{
						const TerrainComponent after = terrain;
						ExecuteComponentEdit(entity, name, before, after);
					}
				};
				// Discrete edits must not retain a reference invalidated by specification assignment.
				discrete("Toggle Terrain Stamp", [&]() { return ImGui::Checkbox("Enabled", &spec.Recipe.Stamps[index].Enabled); });
				int shape = int(spec.Recipe.Stamps[index].Shape);
				discrete("Edit Terrain Stamp Shape", [&]() {
					if (!ImGui::Combo("Shape", &shape, "Ellipse\0Rectangle\0")) return false;
					spec.Recipe.Stamps[index].Shape = TerrainStampShape(shape); return true;
				});
				int operation = int(spec.Recipe.Stamps[index].Operation);
				discrete("Edit Terrain Stamp Operation", [&]() {
					if (!ImGui::Combo("Operation", &operation, "Add\0Set Height\0")) return false;
					spec.Recipe.Stamps[index].Operation = TerrainStampOperation(operation); return true;
				});
				auto continuous = [&](const char* name, auto widget) {
					const TerrainComponent before = terrain;
					if (widget()) TerrainRenderer::Invalidate(terrain);
					CommitComponentWidget(entity, name, m_TerrainEdit, before, terrain);
				};
				continuous("Move Terrain Stamp", [&]() { return ImGui::DragFloat2("Center (X/Z)", &spec.Recipe.Stamps[index].Center.x, 1); });
				continuous("Resize Terrain Stamp", [&]() { return ImGui::DragFloat2("Core Size (X/Z)", &spec.Recipe.Stamps[index].Size.x, 1, 0.001f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp); });
				continuous("Rotate Terrain Stamp", [&]() { return ImGui::DragFloat("Rotation (degrees)", &spec.Recipe.Stamps[index].RotationDegrees, 0.5f, -180, 180, "%.1f", ImGuiSliderFlags_AlwaysClamp); });
				continuous("Edit Terrain Stamp Transition", [&]() { return ImGui::DragFloat("Transition Width", &spec.Recipe.Stamps[index].TransitionWidth, 0.5f, 0, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp); });
				continuous("Edit Terrain Stamp Strength", [&]() { return ImGui::SliderFloat("Strength", &spec.Recipe.Stamps[index].Strength, 0, 1); });
				continuous("Edit Terrain Stamp Height", [&]() { return ImGui::DragFloat(spec.Recipe.Stamps[index].Operation == TerrainStampOperation::Add ? "Height Delta" : "Target Height", &spec.Recipe.Stamps[index].Height, 0.1f); });
				ImGui::TreePop();
			}
			ImGui::PopID(); ImGui::PopID();
		}
	}

	void InspectorPanel::OnImGuiRender()
	{
		ImGui::Begin("Inspector");

		if (!m_Context || !m_Selection)
		{
			ImGui::TextDisabled("No editor context.");
		}
		else if (m_Selection->IsEntitySelected())
		{
			Entity entity = m_Selection->GetEntity();
			if (entity)
			{
				const uint64_t entityID = static_cast<uint64_t>(entity.GetUUID());
				ImGui::PushID(static_cast<int>(entityID >> 32));
				ImGui::PushID(static_cast<int>(entityID & 0xffffffffu));
				DrawComponents(entity);
				ImGui::PopID();
				ImGui::PopID();
			}
			else
				ImGui::TextDisabled("The selected entity is no longer valid.");
		}
		else if (m_Selection->IsAssetSelected())
		{
			const AssetHandle asset = m_Selection->GetAsset();
			const uint64_t assetID = static_cast<uint64_t>(asset);
			ImGui::PushID(static_cast<int>(assetID >> 32));
			ImGui::PushID(static_cast<int>(assetID & 0xffffffffu));
			DrawAssetInspector(asset);
			ImGui::PopID();
			ImGui::PopID();
		}
		else
		{
			ImGui::TextDisabled("Select an entity or asset to inspect it.");
		}

		ImGui::End();
	}

	bool InspectorPanel::ApplyMaterialState(
		const Ref<Material>& material,
		const MaterialState& state)
	{
		if (!material)
			return false;

		const MaterialState previous = material->GetState();
		material->SetState(state);
		if (material->Save())
		{
			m_MaterialSaveError.clear();
			return true;
		}

		material->SetState(previous);
		m_MaterialSaveError = "Could not save material: "
			+ material->GetPath().string();
		GL_CORE_ERROR("{0}", m_MaterialSaveError);
		return false;
	}

	void InspectorPanel::ExecuteMaterialAssetEdit(
		const Ref<Material>& material,
		const char* name,
		const MaterialState& before,
		const MaterialState& after,
		bool alreadyApplied)
	{
		if (!material || SameMaterialState(before, after))
			return;

		auto apply = [this, material](const MaterialState& state) {
			return ApplyMaterialState(material, state);
		};
		auto command = std::make_unique<ValueEditorCommand<MaterialState>>(
			name, before, after, apply);

		if (alreadyApplied)
		{
			if (!material->Save())
			{
				material->SetState(before);
				m_MaterialSaveError = "Could not save material: "
					+ material->GetPath().string();
				GL_CORE_ERROR("{0}", m_MaterialSaveError);
				return;
			}
			m_MaterialSaveError.clear();
			if (m_CommandHistory)
				m_CommandHistory->PushExecuted(std::move(command));
			return;
		}

		if (m_CommandHistory)
			m_CommandHistory->Execute(std::move(command));
		else
			apply(after);
	}

	void InspectorPanel::DrawAssetInspector(AssetHandle handle)
	{
		const AssetMetadata metadata = AssetManager::GetMetadata(handle);
		if (!metadata.IsValid())
		{
			ImGui::TextDisabled("The selected asset is no longer valid.");
			return;
		}

		ImGui::TextUnformatted(metadata.FilePath.filename().string().c_str());
		ImGui::Separator();
		ImGui::Text("Handle: %llu", static_cast<unsigned long long>(handle));
		ImGui::TextWrapped("Path: %s", metadata.FilePath.string().c_str());
		ImGui::Text("Type: %d", static_cast<int>(metadata.Type));

		if (metadata.Type == AssetType::Cubemap)
		{
			Ref<Cubemap> cubemap = AssetManager::GetCubemap(handle);
			if (!cubemap || !cubemap->GetTexture())
			{
				ImGui::TextDisabled("Cubemap asset could not be loaded.");
				return;
			}
			const auto& specification =
				cubemap->GetTexture()->GetSpecification();
			ImGui::Separator();
			ImGui::Text("Source: %s", cubemap->IsHDR()
				? "HDR Equirectangular"
				: "Six Faces");
			if (!cubemap->GetSourcePath().empty())
			{
				ImGui::TextWrapped("Environment: %s",
					cubemap->GetSourcePath().string().c_str());
			}
			ImGui::Text("Format: %s", specification.Format
				== TextureFormat::RGBA16F ? "RGBA16F Linear" : "RGBA8");
			ImGui::Text("Face Size: %u", specification.Size);
			ImGui::Text("Mip Levels: %u", specification.MipLevels);
			ImGui::Text("Runtime Version: %llu",
				static_cast<unsigned long long>(cubemap->GetVersion()));
			if (ImGui::Button("Reload Cubemap"))
				cubemap->Reload();
			return;
		}

		if (metadata.Type == AssetType::TerrainMaterial)
		{
			Ref<TerrainMaterial> material = AssetManager::GetTerrainMaterial(handle);
			if (!material)
			{
				ImGui::TextDisabled("Terrain material asset could not be loaded.");
				return;
			}
			ImGui::Separator();
			ImGui::TextDisabled("Four PBR layers use world-space triplanar mapping.");
			const bool canEdit = m_CommandHistory != nullptr;
			ImGui::BeginDisabled(!canEdit);
			auto& properties = material->GetProperties();
			auto editFloat = [material](const char* label, float& value,
				float speed, float minimum, float maximum) {
				if (ImGui::DragFloat(label, &value, speed, minimum, maximum))
					material->MarkDirty();
			};
			editFloat("Triplanar Sharpness", properties.TriplanarSharpness, 0.1f, 1.0f, 16.0f);
			editFloat("Weight Contrast", properties.WeightContrast, 0.02f, 0.25f, 4.0f);
			editFloat("Height Influence", properties.HeightInfluence, 0.02f, 0.0f, 2.0f);
			editFloat("Slope Influence", properties.SlopeInfluence, 0.02f, 0.0f, 2.0f);
			editFloat("Curvature Influence", properties.CurvatureInfluence, 0.02f, 0.0f, 2.0f);
			editFloat("Moisture Influence", properties.MoistureInfluence, 0.02f, 0.0f, 2.0f);

			auto drawTexture = [material](const char* label, AssetHandle& field,
				TextureColorSpace colorSpace, TextureSemantic semantic) {
				const AssetMetadata textureMetadata = AssetManager::GetMetadata(field);
				const bool valid = textureMetadata.IsValid()
					&& textureMetadata.Type == AssetType::Texture2D;
				ImGui::Text("%s: %s", label, valid
					? textureMetadata.FilePath.filename().string().c_str()
					: "None (drag image here)");
				ImGui::SameLine();
				if (valid && ImGui::SmallButton((std::string("X##") + label).c_str()))
				{
					field = AssetHandle(0);
					material->MarkDirty();
				}
				if (ImGui::BeginDragDropTarget())
				{
					if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
					{
						std::string path((const char*)payload->Data, payload->DataSize - 1);
						const AssetHandle texture = AssetManager::ImportAsset(path);
						if (AssetManager::GetMetadata(texture).Type == AssetType::Texture2D)
						{
							AssetManager::SetTextureMetadata(texture, colorSpace, semantic);
							field = texture;
							material->MarkDirty();
						}
					}
					ImGui::EndDragDropTarget();
				}
			};

			for (size_t index = 0; index < properties.Layers.size(); ++index)
			{
				ImGui::PushID(static_cast<int>(index));
				const auto type = static_cast<TerrainMaterialLayerType>(index);
				if (ImGui::CollapsingHeader(TerrainMaterialLayerTypeToString(type),
					ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& layer = properties.Layers[index];
					if (ImGui::ColorEdit3("Base Color", glm::value_ptr(layer.BaseColor)))
						material->MarkDirty();
					editFloat("Tiling", layer.Tiling, 0.005f, 0.001f, 10.0f);
					editFloat("Metallic", layer.Metallic, 0.01f, 0.0f, 1.0f);
					editFloat("Roughness", layer.Roughness, 0.01f, 0.04f, 1.0f);
					editFloat("Normal Scale", layer.NormalScale, 0.01f, 0.0f, 2.0f);
					editFloat("AO Strength", layer.AOStrength, 0.01f, 0.0f, 1.0f);
					drawTexture("Albedo", layer.AlbedoTexture,
						TextureColorSpace::SRGB, TextureSemantic::Color);
					drawTexture("Normal", layer.NormalTexture,
						TextureColorSpace::Linear, TextureSemantic::Normal);
					drawTexture("AO", layer.AOTexture,
						TextureColorSpace::Linear, TextureSemantic::Data);
				}
				ImGui::PopID();
			}
			if (ImGui::Button("Save Terrain Material"))
				material->Save();
			ImGui::SameLine();
			if (ImGui::Button("Reload from Disk"))
				material->Reload();
			ImGui::EndDisabled();
			return;
		}

		if (metadata.Type != AssetType::Material)
			return;

		Ref<Material> material = AssetManager::GetMaterial(handle);
		if (!material)
		{
			ImGui::TextDisabled("Material asset could not be loaded.");
			return;
		}

		ImGui::Separator();
		ImGui::TextDisabled(
			"Editing this shared asset affects every entity that inherits it.");
		const bool canEditSharedAsset = m_CommandHistory != nullptr;
		if (!canEditSharedAsset)
			ImGui::TextDisabled("Shared assets are read-only while the scene is playing.");
		if (!m_MaterialSaveError.empty())
			ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.2f, 1.0f), "%s",
				m_MaterialSaveError.c_str());
		ImGui::BeginDisabled(!canEditSharedAsset);

		const AssetMetadata shaderMetadata =
			AssetManager::GetMetadata(material->GetShaderHandle());
		const bool hasShader = shaderMetadata.IsValid()
			&& shaderMetadata.Type == AssetType::Shader;
		const std::string shaderName = hasShader
			? shaderMetadata.FilePath.filename().string()
			: "None (drag .glsl here)";
		ImGui::Text("Shader: %s", shaderName.c_str());
		ImGui::SameLine();
		if (hasShader && ImGui::SmallButton("X##AssetMaterialShader"))
		{
			const MaterialState before = material->GetState();
			MaterialState after = before;
			after.ShaderHandle = AssetHandle(0);
			ExecuteMaterialAssetEdit(material, "Clear Material Shader", before, after);
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
			{
				std::string path((const char*)payload->Data, payload->DataSize - 1);
				const AssetHandle shaderHandle = AssetManager::ImportAsset(path);
				if (AssetManager::GetMetadata(shaderHandle).Type == AssetType::Shader)
				{
					const MaterialState before = material->GetState();
					MaterialState after = before;
					after.ShaderHandle = shaderHandle;
					ExecuteMaterialAssetEdit(
						material, "Set Material Shader", before, after);
				}
			}
			ImGui::EndDragDropTarget();
		}

		auto trackContinuousEdit = [this, material](
			const char* name, const MaterialState& beforeWidget) {
			if (ImGui::IsItemActivated())
				m_MaterialAssetEdit.Begin(beforeWidget);
			if (ImGui::IsItemDeactivatedAfterEdit()
				&& m_MaterialAssetEdit.IsActive())
			{
				const MaterialState before = m_MaterialAssetEdit.GetBefore();
				const MaterialState after = material->GetState();
				m_MaterialAssetEdit.Reset();
				ExecuteMaterialAssetEdit(material, name, before, after, true);
			}
		};

		auto& properties = material->GetProperties();
		static const char* alphaModes[] = { "Opaque", "Mask", "Blend" };
		int alphaMode = static_cast<int>(properties.AlphaMode);
		if (ImGui::Combo("Alpha Mode", &alphaMode, alphaModes, 3))
		{
			const MaterialState before = material->GetState();
			MaterialState after = before;
			after.Properties.AlphaMode = static_cast<MaterialAlphaMode>(alphaMode);
			ExecuteMaterialAssetEdit(material, "Set Material Alpha Mode", before, after);
		}

		MaterialState beforeWidget = material->GetState();
		if (ImGui::SliderFloat("Alpha Cutoff", &properties.AlphaCutoff, 0.0f, 1.0f))
			material->MarkDirty();
		trackContinuousEdit("Edit Material Alpha Cutoff", beforeWidget);

		beforeWidget = material->GetState();
		if (ImGui::ColorEdit4("Base Color", glm::value_ptr(properties.BaseColor)))
			material->MarkDirty();
		trackContinuousEdit("Edit Material Base Color", beforeWidget);

		beforeWidget = material->GetState();
		if (ImGui::DragFloat("Material Tiling", &properties.TilingFactor,
			0.05f, 0.01f, 100.0f))
			material->MarkDirty();
		trackContinuousEdit("Edit Material Tiling", beforeWidget);

		beforeWidget = material->GetState();
		if (ImGui::SliderFloat("Metallic", &properties.Metallic, 0.0f, 1.0f))
			material->MarkDirty();
		trackContinuousEdit("Edit Material Metallic", beforeWidget);

		beforeWidget = material->GetState();
		if (ImGui::SliderFloat("Roughness", &properties.Roughness, 0.04f, 1.0f))
			material->MarkDirty();
		trackContinuousEdit("Edit Material Roughness", beforeWidget);

		beforeWidget = material->GetState();
		if (ImGui::SliderFloat("Normal Scale", &properties.NormalScale, 0.0f, 2.0f))
			material->MarkDirty();
		trackContinuousEdit("Edit Material Normal Scale", beforeWidget);

		beforeWidget = material->GetState();
		if (ImGui::SliderFloat("AO Strength", &properties.AOStrength, 0.0f, 1.0f))
			material->MarkDirty();
		trackContinuousEdit("Edit Material AO Strength", beforeWidget);

		beforeWidget = material->GetState();
		if (ImGui::ColorEdit3("Emissive Color", glm::value_ptr(properties.EmissiveColor)))
			material->MarkDirty();
		trackContinuousEdit("Edit Material Emissive Color", beforeWidget);

		beforeWidget = material->GetState();
		if (ImGui::DragFloat("Emissive Strength", &properties.EmissiveStrength,
			0.05f, 0.0f, 100.0f))
			material->MarkDirty();
		trackContinuousEdit("Edit Material Emissive Strength", beforeWidget);

		auto drawTextureSlot = [this, material](const char* label, const char* commandName,
			AssetHandle MaterialProperties::* field, TextureColorSpace colorSpace,
			TextureSemantic semantic) {
			auto& current = material->GetProperties().*field;
			const AssetMetadata textureMetadata = AssetManager::GetMetadata(current);
			const bool hasTexture = textureMetadata.IsValid()
				&& textureMetadata.Type == AssetType::Texture2D;
			const std::string textureName = hasTexture
				? textureMetadata.FilePath.filename().string() : "None (drag image here)";
			ImGui::PushID(label);
			ImGui::Text("%s: %s", label, textureName.c_str());
			ImGui::SameLine();
			if (hasTexture && ImGui::SmallButton("X"))
			{
				const MaterialState before = material->GetState();
				MaterialState after = before;
				after.Properties.*field = AssetHandle(0);
				ExecuteMaterialAssetEdit(material, commandName, before, after);
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
						const MaterialState before = material->GetState();
						MaterialState after = before;
						after.Properties.*field = textureHandle;
						ExecuteMaterialAssetEdit(material, commandName, before, after);
					}
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		};
		drawTextureSlot("Base Color Texture", "Edit Base Color Texture",
			&MaterialProperties::BaseColorTexture,
			TextureColorSpace::SRGB, TextureSemantic::Color);
		drawTextureSlot("Normal Texture", "Edit Normal Texture",
			&MaterialProperties::NormalTexture,
			TextureColorSpace::Linear, TextureSemantic::Normal);
		drawTextureSlot("AO Texture", "Edit AO Texture",
			&MaterialProperties::AOTexture,
			TextureColorSpace::Linear, TextureSemantic::Data);
		drawTextureSlot("Emissive Texture", "Edit Emissive Texture",
			&MaterialProperties::EmissiveTexture,
			TextureColorSpace::SRGB, TextureSemantic::Color);
		ImGui::EndDisabled();
	}
}
