#pragma once

#include "Glimmer/Scene/Components.h"

namespace gl {
	class TerrainRenderer
	{
	public:
		static void Draw(TerrainComponent& component, const glm::mat4& transform,
			const glm::mat4& viewProjection, const glm::vec3& cameraPosition, int entityID);
		static void Invalidate(TerrainComponent& component);
	};
}