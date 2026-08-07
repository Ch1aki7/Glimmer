#pragma once

#include "Glimmer/Asset/Asset.h"
#include "Glimmer/Renderer/MaterialInstance.h"
#include <glm/glm.hpp>

namespace gl {

	class Renderer3D
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const glm::mat4& viewProjection, const glm::vec3& cameraPosition);
		static void SubmitModel(
			const glm::mat4& transform,
			AssetHandle modelHandle,
			AssetHandle materialHandle,
			int entityID,
			const MaterialOverrides* overrides = nullptr);
		static void EndScene();

		struct Statistics
		{
			uint32_t SubmittedModels = 0;
			uint32_t SubmittedItems = 0;
			uint32_t SkippedModels = 0;
			uint32_t DrawCalls = 0;
			uint32_t InstancedDrawCalls = 0;
			uint32_t IndividualDrawCalls = 0;
			uint32_t BatchCount = 0;
			uint32_t RenderedItems = 0;
			uint32_t InstanceCount = 0;
			uint32_t MaterialCacheHits = 0;
			uint32_t MaterialCacheMisses = 0;
			uint32_t ShaderBinds = 0;
			uint32_t TextureBinds = 0;
			uint32_t ImmediateModeShaderBinds = 0;
			uint32_t ImmediateModeTextureBinds = 0;

			uint32_t GetSavedDrawCalls() const
			{
				return SubmittedItems > DrawCalls ? SubmittedItems - DrawCalls : 0;
			}

			uint32_t GetSavedShaderBinds() const
			{
				return ImmediateModeShaderBinds > ShaderBinds
					? ImmediateModeShaderBinds - ShaderBinds : 0;
			}
			uint32_t GetSavedTextureBinds() const
			{
				return ImmediateModeTextureBinds > TextureBinds
					? ImmediateModeTextureBinds - TextureBinds : 0;
			}
		};

		static void ResetStats();
		static Statistics GetStats();
	};

}
