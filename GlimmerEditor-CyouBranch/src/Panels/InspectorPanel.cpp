#include "InspectorPanel.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/Material.h"
#include "Glimmer/Renderer/Cubemap.h"
#include "Glimmer/Terrain/TerrainMaterial.h"

#include <glm/gtc/type_ptr.hpp>

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
				&& SameMaterialProperties(left.Properties, right.Properties);
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
				DrawComponents(entity);
			else
				ImGui::TextDisabled("The selected entity is no longer valid.");
		}
		else if (m_Selection->IsAssetSelected())
		{
			DrawAssetInspector(m_Selection->GetAsset());
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
