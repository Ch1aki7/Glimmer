#include "InspectorPanel.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/Material.h"

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
				&& left.TilingFactor == right.TilingFactor
				&& left.Metallic == right.Metallic
				&& left.Roughness == right.Roughness;
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
		MaterialState beforeWidget = material->GetState();
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

		const AssetMetadata textureMetadata =
			AssetManager::GetMetadata(properties.BaseColorTexture);
		const bool hasTexture = textureMetadata.IsValid()
			&& textureMetadata.Type == AssetType::Texture2D;
		const std::string textureName = hasTexture
			? textureMetadata.FilePath.filename().string()
			: "None (drag image here)";
		ImGui::Text("Base Color Texture: %s", textureName.c_str());
		ImGui::SameLine();
		if (hasTexture && ImGui::SmallButton("X##AssetMaterialTexture"))
		{
			const MaterialState before = material->GetState();
			MaterialState after = before;
			after.Properties.BaseColorTexture = AssetHandle(0);
			ExecuteMaterialAssetEdit(
				material, "Clear Material Texture", before, after);
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
			{
				std::string path((const char*)payload->Data, payload->DataSize - 1);
				const AssetHandle textureHandle = AssetManager::ImportAsset(path);
				if (AssetManager::GetMetadata(textureHandle).Type == AssetType::Texture2D)
				{
					const MaterialState before = material->GetState();
					MaterialState after = before;
					after.Properties.BaseColorTexture = textureHandle;
					ExecuteMaterialAssetEdit(
						material, "Set Material Texture", before, after);
				}
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::EndDisabled();
	}
}
