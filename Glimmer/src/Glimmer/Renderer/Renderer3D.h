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
		static void DrawModel(
			const glm::mat4& transform,
			AssetHandle modelHandle,
			AssetHandle materialHandle,
			int entityID,
			const MaterialOverrides* overrides = nullptr);
	};

}
