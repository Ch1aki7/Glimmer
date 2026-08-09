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
		static void Shutdown();
		static bool BeginDirectional(
			const glm::vec3& lightDirection,
			const glm::vec3& focusPosition,
			uint32_t resolution,
			float distance,
			float bias);
		static void SubmitModel(AssetHandle modelHandle, const glm::mat4& transform);
		static void SubmitTerrain(TerrainComponent& terrain, const glm::mat4& transform);
		static void EndDirectional();
		static void Disable();

		static void BindForLighting(const Ref<Shader>& shader, uint32_t textureSlot);
		static bool IsEnabled();
	};
}
