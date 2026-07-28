#include "InspectorPanel.h"

#include "Glimmer/Asset/AssetManager.h"

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
			if (entity && m_EntityDrawer)
				m_EntityDrawer(entity);
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
	}
}
