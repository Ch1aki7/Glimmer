#include "DebugPanel.h"

#include "Glimmer/Renderer/ShadowRenderer.h"
#include "Glimmer/Renderer/TerrainRenderer.h"

#include <imgui.h>
#include <utility>

namespace gl {

	void DebugPanel::SetTemporarySceneCallbacks(
		InstancingLabTool::ActivateSceneCallback activateScene,
		InstancingLabTool::ExitSceneCallback exitScene,
		InstancingLabTool::SelectEntityCallback selectEntity,
		InstancingLabTool::FrameSceneCallback frameScene)
	{
		m_InstancingLab.SetCallbacks(
			activateScene, exitScene, selectEntity, std::move(frameScene));
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
		const ShadowRenderer::Statistics shadowStatistics =
			ShadowRenderer::GetStatistics();
		const TerrainRenderer::Statistics terrainStatistics =
			TerrainRenderer::GetStatistics();
		const TerrainHydrologyGPUStatistics hydrologyStatistics =
			TerrainRenderer::GetHydrologyStatistics();
		const TerrainHydrologyGPUValidationResult hydrologyValidation =
			TerrainRenderer::GetHydrologyValidationResult();
		m_InstancingLab.UpdateShadowBenchmark(shadowStatistics);
		m_PBRMaterialLab.UpdateValidation(statistics);
		m_TerrainSamplingBenchmark.Update(terrainStatistics);
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
				ImGui::Separator();
				ImGui::TextUnformatted("Terrain");
				ImGui::Text("Draw Calls: %u", terrainStatistics.DrawCalls);
				ImGui::Text("Candidate / Submitted: %u / %u",
					terrainStatistics.CandidateChunks,
					terrainStatistics.SubmittedChunks);
				ImGui::Text("Frustum Culled / Shared Meshes: %u / %u",
					terrainStatistics.CulledChunks,
					terrainStatistics.SharedMeshes);
				ImGui::Text("LOD Chunks (0 / 1 / 2): %u / %u / %u",
					terrainStatistics.LOD0Chunks,
					terrainStatistics.LOD1Chunks,
					terrainStatistics.LOD2Chunks);
				ImGui::Text("Submitted Triangles: %llu",
					static_cast<unsigned long long>(terrainStatistics.SubmittedTriangles));
				glm::vec2 lodDistances = TerrainRenderer::GetLODDistances();
				if (ImGui::DragFloat2("LOD Distances", &lodDistances.x,
					1.0f, 1.0f, 2000.0f, "%.0f"))
					TerrainRenderer::SetLODDistances(
						lodDistances.x, lodDistances.y);
				bool visualizeLODs =
					TerrainRenderer::IsLODVisualizationEnabled();
				if (ImGui::Checkbox("Visualize Terrain LODs", &visualizeLODs))
					TerrainRenderer::SetLODVisualizationEnabled(visualizeLODs);
				ImGui::TextDisabled("LOD0 Red, LOD1 Green, LOD2 Blue");
				ImGui::Text("Bound Material Textures: %u",
					terrainStatistics.BoundMaterialTextures);
				if (terrainStatistics.GpuTimingAvailable)
					ImGui::Text("GPU Time: %.3f ms",
						terrainStatistics.GpuMilliseconds);
				else
					ImGui::TextDisabled("GPU Time: pending");
				int samplingMode = static_cast<int>(
					TerrainRenderer::GetSamplingMode());
				const char* samplingModes[] = {
					"Full 4 Layers",
					"Top 2 Layers",
					"Top 2 + Dominant Normal/AO",
					"Auto Distance"
				};
				if (ImGui::Combo("Sampling", &samplingMode,
					samplingModes, IM_ARRAYSIZE(samplingModes)))
				{
					TerrainRenderer::SetSamplingMode(
						static_cast<TerrainRenderer::SamplingMode>(samplingMode));
				}
				if (TerrainRenderer::GetSamplingMode()
					== TerrainRenderer::SamplingMode::AutomaticDistance)
				{
					float detailDistance = TerrainRenderer::GetDetailDistance();
					if (ImGui::DragFloat("Detail Distance", &detailDistance,
						1.0f, 1.0f, 10000.0f))
						TerrainRenderer::SetDetailDistance(detailDistance);
				}
				m_TerrainSamplingBenchmark.OnImGuiRender(terrainStatistics);
				ImGui::SeparatorText("Runtime Hydrology");
				bool hydrologyPlaying = TerrainRenderer::IsHydrologyPlaying();
				if (ImGui::Checkbox("Play##Hydrology", &hydrologyPlaying))
					TerrainRenderer::SetHydrologyPlaying(hydrologyPlaying);
				ImGui::SameLine();
				ImGui::BeginDisabled(hydrologyPlaying);
				if (ImGui::Button("Single Step##Hydrology"))
					TerrainRenderer::RequestHydrologySingleStep();
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::Button("Reset##Hydrology"))
					TerrainRenderer::RequestHydrologyReset();
				float rainfall = TerrainRenderer::GetHydrologyRainfall();
				if (ImGui::DragFloat("Rainfall##Hydrology", &rainfall,
					0.002f, 0.0f, 5.0f, "%.3f depth/s"))
					TerrainRenderer::SetHydrologyRainfall(rainfall);
				bool visualizeHydrology =
					TerrainRenderer::IsHydrologyVisualizationEnabled();
				if (ImGui::Checkbox("Visualize Water Depth##Hydrology",
					&visualizeHydrology))
					TerrainRenderer::SetHydrologyVisualizationEnabled(
						visualizeHydrology);
				if (ImGui::Button("Validate / Readback##Hydrology"))
					TerrainRenderer::RequestHydrologyReadback();
				ImGui::SameLine();
				if (ImGui::Button("Run GPU Contract##Hydrology"))
					TerrainRenderer::RequestHydrologyContractValidation();
				ImGui::Text("Steps / Sim Time: %llu / %.2f s",
					static_cast<unsigned long long>(hydrologyStatistics.StepCount),
					hydrologyStatistics.SimulatedTime);
				ImGui::Text("Accumulator / Dropped: %.4f / %.4f s",
					hydrologyStatistics.Accumulator,
					hydrologyStatistics.DroppedTime);
				if (hydrologyStatistics.ReadbackAvailable)
				{
					ImGui::Text("Water Volume / Error: %.6f / %.3e",
						hydrologyStatistics.WaterVolume,
						hydrologyStatistics.MassError);
					ImGui::Text("Depth Min/Max: %.6f / %.6f",
						hydrologyStatistics.MinimumWaterDepth,
						hydrologyStatistics.MaximumWaterDepth);
					ImGui::Text("Max Speed: %.6f",
						hydrologyStatistics.MaximumSpeed);
					ImGui::TextColored(
						hydrologyStatistics.Finite
							? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
							: ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
						hydrologyStatistics.Finite ? "Finite: PASS" : "Finite: FAIL");
				}
				else
					ImGui::TextDisabled("Press Validate / Readback for mass statistics");
				if (hydrologyValidation.Attempted)
				{
					ImGui::TextColored(
						hydrologyValidation.Passed
							? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
							: ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
						hydrologyValidation.Passed
							? "GPU Contract: PASS" : "GPU Contract: FAIL");
					ImGui::Text("Relative Mass Error: %.3e",
						hydrologyValidation.RelativeMassError);
					ImGui::Text("Basin / Rim Max: %.6f / %.6f",
						hydrologyValidation.BasinDepth,
						hydrologyValidation.MaximumRimDepth);
					ImGui::Text("Frame Partition Delta: %.3e",
						hydrologyValidation.MaximumPartitionDifference);
				}
				else
					ImGui::TextDisabled("GPU contract validation not run");
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

	bool DebugPanel::GenerateInstancingLabForShadowBenchmark()
	{
		if (m_PBRMaterialLab.IsActive())
			return false;
		m_Open = true;
		return m_InstancingLab.GenerateForShadowBenchmark();
	}

	bool DebugPanel::GenerateInstancingLabForShadowVisualValidation(bool casterCloseup)
	{
		if (m_PBRMaterialLab.IsActive())
			return false;
		m_Open = true;
		return m_InstancingLab.GenerateForShadowVisualValidation(casterCloseup);
	}

}
