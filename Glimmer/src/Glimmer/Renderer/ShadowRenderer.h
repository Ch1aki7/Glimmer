#pragma once

#include "Glimmer/Asset/Asset.h"
#include "Glimmer/Core/Core.h"
#include <glm/glm.hpp>

namespace gl {
	class Shader;
	struct TerrainComponent;

	class ShadowRenderer
	{
	public:
		static constexpr uint32_t MaxCascades = 4;

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
		static void SubmitModel(AssetHandle modelHandle, const glm::mat4& transform);
		static void SubmitTerrain(TerrainComponent& terrain, const glm::mat4& transform);
		static void EndCascade();
		static void EndDirectional();
		static void Disable();

		static void BindForLighting(const Ref<Shader>& shader, uint32_t textureSlot);
		static bool IsEnabled();
	};
}
