#pragma once

#include "../Debug/InstancingLabTool.h"
#include "../Debug/PBRMaterialLabTool.h"

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
	};

}
