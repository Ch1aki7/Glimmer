#pragma once

#include "../Debug/InstancingLabTool.h"
#include "../Debug/PBRMaterialLabTool.h"
#include "../Debug/TerrainSamplingBenchmarkTool.h"

namespace gl {

	class DebugPanel
	{
	public:
		void SetOpen(bool open) { m_Open = open; }
		bool IsOpen() const { return m_Open; }

		void SetTemporarySceneCallbacks(
			InstancingLabTool::ActivateSceneCallback activateScene,
			InstancingLabTool::ExitSceneCallback exitScene,
			InstancingLabTool::SelectEntityCallback selectEntity,
			InstancingLabTool::FrameSceneCallback frameScene);
		void SetDefaultAssets(
			AssetHandle modelHandle,
			AssetHandle materialHandle,
			AssetHandle skyboxHandle,
			AssetHandle sphereModelHandle,
			AssetHandle normalTextureHandle,
			AssetHandle aoTextureHandle,
			AssetHandle emissiveTextureHandle);

		void OnImGuiRender(const Renderer3D::Statistics& statistics);
		bool GeneratePBRMaterialLabForValidation();
		bool GenerateInstancingLabForShadowBenchmark();
		bool GenerateInstancingLabForShadowVisualValidation(bool casterCloseup = false);
		bool StartTerrainSamplingBenchmark(bool waitForTexturedTerrain = false)
		{
			m_Open = true;
			return m_TerrainSamplingBenchmark.Start(waitForTexturedTerrain);
		}
		bool IsTerrainSamplingBenchmarkComplete() const
		{
			return m_TerrainSamplingBenchmark.IsComplete();
		}
		bool IsShadowBenchmarkComplete() const
		{
			return m_InstancingLab.IsShadowBenchmarkComplete();
		}
		void ExitTemporaryTools();
		bool IsTemporarySceneActive() const
		{
			return m_InstancingLab.IsActive() || m_PBRMaterialLab.IsActive();
		}

	private:
		bool m_Open = false;
		InstancingLabTool m_InstancingLab;
		PBRMaterialLabTool m_PBRMaterialLab;
		TerrainSamplingBenchmarkTool m_TerrainSamplingBenchmark;
	};

}
