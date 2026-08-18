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
				int visualizationMode = static_cast<int>(
					TerrainRenderer::GetHydrologyVisualizationMode());
				const char* visualizationModes[] = {
					"None", "Water Depth", "Suspended Sediment",
					"Sediment Capacity", "Sediment Saturation"
				};
				if (ImGui::Combo("Visualization##Hydrology", &visualizationMode,
					visualizationModes, IM_ARRAYSIZE(visualizationModes)))
				{
					TerrainRenderer::SetHydrologyVisualizationMode(
						static_cast<TerrainRenderer::HydrologyVisualizationMode>(
							visualizationMode));
				}
				float sedimentSeed =
					TerrainRenderer::GetHydrologySedimentSeedDensity();
				if (ImGui::DragFloat("Sediment Seed##Hydrology", &sedimentSeed,
					0.01f, 0.0f, 1000.0f, "%.3f mass/area"))
					TerrainRenderer::SetHydrologySedimentSeedDensity(sedimentSeed);
				float capacityScale =
					TerrainRenderer::GetHydrologySedimentCapacityScale();
				if (ImGui::DragFloat("Capacity Scale##Hydrology", &capacityScale,
					0.01f, 0.0f, 1000.0f, "%.3f"))
				{
					TerrainRenderer::SetHydrologySedimentCapacityScale(
						capacityScale);
				}
				float erosionRate = TerrainRenderer::GetHydrologyErosionRate();
				if (ImGui::DragFloat("Erosion Rate##Hydrology", &erosionRate,
					0.01f, 0.0f, 1000.0f, "%.3f /s"))
					TerrainRenderer::SetHydrologyErosionRate(erosionRate);
				float depositionRate =
					TerrainRenderer::GetHydrologyDepositionRate();
				if (ImGui::DragFloat(
					"Deposition Rate##Hydrology", &depositionRate,
					0.01f, 0.0f, 1000.0f, "%.3f /s"))
				{
					TerrainRenderer::SetHydrologyDepositionRate(
						depositionRate);
				}
				float terrainDensity =
					TerrainRenderer::GetHydrologyTerrainDensity();
				if (ImGui::DragFloat("Terrain Density##Hydrology",
					&terrainDensity, 0.01f, 0.000001f, 1000000.0f, "%.3f"))
					TerrainRenderer::SetHydrologyTerrainDensity(terrainDensity);
				float maximumErosionDepth =
					TerrainRenderer::GetHydrologyMaximumErosionDepth();
				if (ImGui::DragFloat("Max Erosion Depth##Hydrology",
					&maximumErosionDepth, 0.01f, 0.0f, 10000.0f, "%.3f"))
				{
					TerrainRenderer::SetHydrologyMaximumErosionDepth(
						maximumErosionDepth);
				}
				float maximumHeightChange =
					TerrainRenderer::GetHydrologyMaximumHeightChange();
				if (ImGui::DragFloat("Max Height Step##Hydrology",
					&maximumHeightChange, 0.0001f, 0.0f, 1000.0f, "%.5f"))
				{
					TerrainRenderer::SetHydrologyMaximumHeightChange(
						maximumHeightChange);
				}
				ImGui::SameLine();
				if (ImGui::Button("Apply Seed##Hydrology"))
					TerrainRenderer::RequestHydrologySedimentSeed();
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
					ImGui::Text("Sediment Mass / Error: %.6f / %.3e",
						hydrologyStatistics.SedimentMass,
						hydrologyStatistics.SedimentMassError);
					ImGui::Text("Sediment Min/Max: %.6f / %.6f",
						hydrologyStatistics.MinimumSediment,
						hydrologyStatistics.MaximumSediment);
					ImGui::Text("Capacity Min/Max: %.6f / %.6f",
						hydrologyStatistics.MinimumSedimentCapacity,
						hydrologyStatistics.MaximumSedimentCapacity);
					ImGui::Text("Saturation Min/Max: %.6f / %.6f",
						hydrologyStatistics.MinimumSedimentSaturation,
						hydrologyStatistics.MaximumSedimentSaturation);
					ImGui::Text("Terrain Height Min/Max: %.6f / %.6f",
						hydrologyStatistics.MinimumTerrainHeight,
						hydrologyStatistics.MaximumTerrainHeight);
					ImGui::Text("Net Eroded / Deposited: %.6f / %.6f",
						hydrologyStatistics.ErodedMass,
						hydrologyStatistics.DepositedMass);
					ImGui::Text("Terrain + Sediment Error: %.3e",
						hydrologyStatistics.TerrainSedimentMassError);
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
					ImGui::Text("Sediment Mass Error: %.3e",
						hydrologyValidation.RelativeSedimentMassError);
					ImGui::Text("Sediment Source / Downstream: %.6f / %.6f",
						hydrologyValidation.SourceSediment,
						hydrologyValidation.DownstreamSediment);
					ImGui::Text("Sediment Partition Delta: %.3e",
						hydrologyValidation.MaximumSedimentPartitionDifference);
					ImGui::Text("Capacity / Saturation Max: %.6f / %.6f",
						hydrologyValidation.MaximumSedimentCapacity,
						hydrologyValidation.MaximumSedimentSaturation);
					ImGui::Text("Capacity / Saturation Delta: %.3e / %.3e",
						hydrologyValidation.MaximumCapacityPartitionDifference,
						hydrologyValidation.MaximumSaturationPartitionDifference);
					ImGui::Text("Erosion Mass Error: %.3e",
						hydrologyValidation.RelativeTerrainSedimentMassError);
					ImGui::Text("Eroded / Deposited Height: %.6f / %.6f",
						hydrologyValidation.ErodedHeight,
						hydrologyValidation.DepositedHeight);
					ImGui::Text("Erosion Height / Sediment Delta: %.3e / %.3e",
						hydrologyValidation.MaximumErosionHeightPartitionDifference,
						hydrologyValidation.MaximumErosionSedimentPartitionDifference);
					ImGui::Text("Erosion Reset: %s",
						hydrologyValidation.ErosionResetValid ? "PASS" : "FAIL");
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
