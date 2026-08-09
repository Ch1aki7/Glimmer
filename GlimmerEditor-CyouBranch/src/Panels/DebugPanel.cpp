#include "DebugPanel.h"

#include "Glimmer/Renderer/ShadowRenderer.h"

#include <imgui.h>
#include <utility>

namespace gl {

	void DebugPanel::SetTemporarySceneCallbacks(
		InstancingLabTool::ActivateSceneCallback activateScene,
		InstancingLabTool::ExitSceneCallback exitScene,
		InstancingLabTool::SelectEntityCallback selectEntity)
	{
		m_InstancingLab.SetCallbacks(
			activateScene, exitScene, selectEntity);
		m_PBRMaterialLab.SetCallbacks(
			std::move(activateScene), std::move(exitScene), std::move(selectEntity));
	}

	void DebugPanel::SetDefaultAssets(
		AssetHandle modelHandle,
		AssetHandle materialHandle,
		AssetHandle skyboxHandle,
		AssetHandle sphereModelHandle,
		AssetHandle normalTextureHandle,
		AssetHandle aoTextureHandle,
		AssetHandle emissiveTextureHandle)
	{
		m_InstancingLab.SetDefaultAssets(
			modelHandle, materialHandle, skyboxHandle);
		m_PBRMaterialLab.SetDefaultAssets(
			sphereModelHandle, materialHandle, skyboxHandle,
			normalTextureHandle, aoTextureHandle, emissiveTextureHandle);
	}

	void DebugPanel::OnImGuiRender(const Renderer3D::Statistics& statistics)
	{
		m_PBRMaterialLab.UpdateValidation(statistics);
		if (!m_Open)
			return;
		if (!ImGui::Begin("Debug", &m_Open))
		{
			ImGui::End();
			return;
		}

		if (ImGui::BeginTabBar("DebugTools"))
		{
			if (ImGui::BeginTabItem("Overview"))
			{
				ImGui::TextUnformatted("Renderer3D");
				ImGui::Text("Items / Draw Calls: %u / %u",
					statistics.SubmittedItems, statistics.DrawCalls);
				ImGui::Text("Instanced / Individual: %u / %u",
					statistics.InstancedDrawCalls, statistics.IndividualDrawCalls);
				ImGui::Text("Instances / Saved Draws: %u / %u",
					statistics.InstanceCount, statistics.GetSavedDrawCalls());
				ImGui::Text("Material Cache Hit / Miss: %u / %u",
					statistics.MaterialCacheHits, statistics.MaterialCacheMisses);
				const ShadowRenderer::Statistics shadowStatistics =
					ShadowRenderer::GetStatistics();
				ImGui::Separator();
				ImGui::TextUnformatted("Directional Shadows");
				ImGui::Text("Cascades: %u", shadowStatistics.CascadePasses);
				ImGui::Text("Candidate / Rendered: %u / %u",
					shadowStatistics.CandidateDraws, shadowStatistics.RenderedDraws);
				ImGui::Text("Frustum Culled: %u", shadowStatistics.CulledDraws);
				ImGui::Text("Draw Calls (Instanced / Individual): %u (%u / %u)",
					shadowStatistics.DrawCalls,
					shadowStatistics.InstancedDrawCalls,
					shadowStatistics.IndividualDrawCalls);
				ImGui::Text("Instances / Saved Draws: %u / %u",
					shadowStatistics.InstanceCount,
					shadowStatistics.GetSavedDrawCalls());
				if (shadowStatistics.GpuTimingAvailable)
					ImGui::Text("GPU Time: %.3f ms", shadowStatistics.GpuMilliseconds);
				else
					ImGui::TextDisabled("GPU Time: pending");
				bool visualizeCascades =
					ShadowRenderer::IsCascadeDebugVisualizationEnabled();
				if (ImGui::Checkbox("Visualize Cascades", &visualizeCascades))
					ShadowRenderer::SetCascadeDebugVisualization(visualizeCascades);
				ImGui::TextDisabled("1 Red, 2 Green, 3 Blue, 4 Yellow");
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Rendering"))
			{
				ImGui::BeginDisabled(m_PBRMaterialLab.IsActive());
				m_InstancingLab.OnImGuiRender(statistics);
				ImGui::EndDisabled();
				m_PBRMaterialLab.OnImGuiRender(
					statistics, m_InstancingLab.IsActive());
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}

	void DebugPanel::ExitTemporaryTools()
	{
		m_InstancingLab.Exit();
		m_PBRMaterialLab.Exit();
	}

	bool DebugPanel::GeneratePBRMaterialLabForValidation()
	{
		if (m_InstancingLab.IsActive())
			return false;
		m_Open = true;
		return m_PBRMaterialLab.GenerateForValidation();
	}

}
