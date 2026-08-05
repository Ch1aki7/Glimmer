#include "InspectorPanel.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/Material.h"

#include <glm/gtc/type_ptr.hpp>

namespace gl
{
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
		ImGui::TextDisabled("Editing this shared asset affects every entity that inherits it.");
		bool changed = false;

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
			material->SetShaderHandle(AssetHandle(0));
			changed = true;
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
			{
				std::string path((const char*)payload->Data, payload->DataSize - 1);
				const AssetHandle shaderHandle = AssetManager::ImportAsset(path);
				if (AssetManager::GetMetadata(shaderHandle).Type == AssetType::Shader)
				{
					material->SetShaderHandle(shaderHandle);
					changed = true;
				}
			}
			ImGui::EndDragDropTarget();
		}

		auto& properties = material->GetProperties();
		changed |= ImGui::ColorEdit4("Base Color", glm::value_ptr(properties.BaseColor));
		changed |= ImGui::DragFloat("Material Tiling", &properties.TilingFactor,
			0.05f, 0.01f, 100.0f);
		changed |= ImGui::SliderFloat("Metallic", &properties.Metallic, 0.0f, 1.0f);
		changed |= ImGui::SliderFloat("Roughness", &properties.Roughness, 0.04f, 1.0f);

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
			properties.BaseColorTexture = AssetHandle(0);
			changed = true;
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
			{
				std::string path((const char*)payload->Data, payload->DataSize - 1);
				const AssetHandle textureHandle = AssetManager::ImportAsset(path);
				if (AssetManager::GetMetadata(textureHandle).Type == AssetType::Texture2D)
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
}
