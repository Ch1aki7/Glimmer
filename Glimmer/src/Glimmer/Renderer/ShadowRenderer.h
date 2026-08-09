#pragma once

#include "Glimmer/Asset/Asset.h"
#include "Glimmer/Core/Core.h"
#include <glm/glm.hpp>

namespace gl {
	class Shader;
	struct MaterialOverrides;
	struct TerrainComponent;

	class ShadowRenderer
	{
	public:
		static constexpr uint32_t MaxCascades = 4;
		struct Statistics
		{
			uint32_t CascadePasses = 0;
			uint32_t CandidateDraws = 0;
			uint32_t CulledDraws = 0;
			uint32_t RenderedDraws = 0;
			uint32_t DrawCalls = 0;
			uint32_t InstancedDrawCalls = 0;
			uint32_t IndividualDrawCalls = 0;
			uint32_t InstanceCount = 0;

			uint32_t GetSavedDrawCalls() const
			{
				return RenderedDraws > DrawCalls ? RenderedDraws - DrawCalls : 0;
			}
		};

		static void Shutdown();
		static bool BeginDirectional(
			const glm::vec3& lightDirection,
			const glm::mat4& cameraView,
			const glm::mat4& cameraProjection,
			float cameraNear,
			float cameraFar,
			uint32_t resolution,
			float distance,
			float bias,
			uint32_t cascadeCount,
			float splitLambda,
			float cascadeBlend);
		static bool BeginCascade(uint32_t cascadeIndex);
		static void SubmitModel(
			AssetHandle modelHandle,
			const glm::mat4& transform,
			AssetHandle materialHandle = AssetHandle(0),
			const MaterialOverrides* overrides = nullptr);
		static void SubmitTerrain(TerrainComponent& terrain, const glm::mat4& transform);
		static void EndCascade();
		static void EndDirectional();
		static void Disable();

		static void BindForLighting(const Ref<Shader>& shader, uint32_t textureSlot);
		static bool IsEnabled();
		static void SetCascadeDebugVisualization(bool enabled);
		static bool IsCascadeDebugVisualizationEnabled();
		static Statistics GetStatistics();
		static bool IntersectsClipFrustum(
			const glm::vec3& boundsMin,
			const glm::vec3& boundsMax,
			const glm::mat4& transform,
			const glm::mat4& viewProjection);
	};
}
