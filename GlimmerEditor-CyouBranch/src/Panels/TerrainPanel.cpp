#include "TerrainPanel.h"

#include <imgui.h>

namespace gl {

	void TerrainPanel::SetContext(TerrainGenerator* generator)
	{
		m_Generator = generator;
		m_Dirty = true;
	}

	void TerrainPanel::OnUpdate()
	{
		if (!m_Generator)
			return;

		const ShaderReloadResult reloadResult = m_Generator->ReloadShaderIfChanged();
		if (reloadResult.Attempted && reloadResult.Success)
			m_Dirty = true;

		if (m_Dirty && m_AutoRegenerate)
		{
			m_Generator->Generate(m_Settings);
			m_Dirty = false;
		}
	}

	void TerrainPanel::OnImGuiRender()
	{
		ImGui::Begin("Terrain");

		if (!m_Generator)
		{
			ImGui::TextDisabled("No TerrainGenerator is assigned.");
			ImGui::End();
			return;
		}

		bool changed = false;
		const auto& currentSpecification = m_Generator->GetGridSpecification();
		const char* resolutions[] = { "256 x 256", "512 x 512", "1024 x 1024" };
		int resolutionIndex = currentSpecification.Width <= 256 ? 0
			: (currentSpecification.Width <= 512 ? 1 : 2);
		if (ImGui::Combo("Resolution", &resolutionIndex, resolutions, IM_ARRAYSIZE(resolutions)))
		{
			const uint32_t resolution = 256u << resolutionIndex;
			m_Generator->Resize(resolution, resolution);
			changed = true;
		}

		changed |= ImGui::DragInt("Seed", &m_Settings.Seed, 1.0f);
		changed |= ImGui::SliderInt("Octaves", &m_Settings.Octaves, 1, 12);
		changed |= ImGui::DragFloat("Frequency", &m_Settings.Frequency, 0.01f, 0.05f, 32.0f);
		changed |= ImGui::DragFloat("Lacunarity", &m_Settings.Lacunarity, 0.01f, 1.0f, 4.0f);
		changed |= ImGui::DragFloat("Persistence", &m_Settings.Persistence, 0.01f, 0.05f, 0.95f);
		changed |= ImGui::DragFloat("Domain Warp", &m_Settings.DomainWarp, 0.01f, 0.0f, 4.0f);
		changed |= ImGui::SliderFloat("Ridge Strength", &m_Settings.RidgeStrength, 0.0f, 1.0f);
		changed |= ImGui::SliderFloat("Continent Scale", &m_Settings.ContinentScale, 0.05f, 1.0f);
		changed |= ImGui::SliderFloat("Erosion Strength", &m_Settings.ErosionStrength, 0.0f, 0.5f);
		changed |= ImGui::SliderFloat("Detail Strength", &m_Settings.DetailStrength, 0.0f, 0.25f);
		changed |= ImGui::DragFloat2("Offset", &m_Settings.Offset.x, 0.005f);
		m_Dirty |= changed;

		ImGui::Checkbox("Auto Regenerate", &m_AutoRegenerate);
		ImGui::SameLine();
		if (ImGui::Button("Regenerate"))
		{
			m_Generator->Generate(m_Settings);
			m_Dirty = false;
		}

		const auto& specification = m_Generator->GetGridSpecification();
		ImGui::Text("Resolution: %u x %u", specification.Width, specification.Height);
		ImGui::Text("State: %s", m_Dirty ? "Pending" : "Up to date");

		const uint32_t textureID = m_Generator->GetHeightMap()->GetRendererID();
		ImGui::Image(
			reinterpret_cast<void*>(static_cast<uintptr_t>(textureID)),
			ImVec2(192.0f, 192.0f),
			ImVec2(0.0f, 1.0f),
			ImVec2(1.0f, 0.0f));

		ImGui::End();
	}

}


