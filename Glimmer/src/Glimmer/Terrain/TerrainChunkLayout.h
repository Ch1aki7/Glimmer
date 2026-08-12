#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

namespace gl {

	struct TerrainChunkRegion
	{
		glm::uvec2 Coordinate{ 0 };
		glm::vec2 UVOffset{ 0.0f };
		glm::vec2 UVScale{ 1.0f };
		glm::vec2 LocalOffset{ 0.0f };
		float LocalScale = 1.0f;
		float WorldSize = 0.0f;
	};

	class TerrainChunkLayout
	{
	public:
		static constexpr uint32_t AxisCount = 3;
		static constexpr uint32_t ChunkCount = AxisCount * AxisCount;

		static uint32_t CalculateSharedMeshResolution(
			uint32_t terrainMeshResolution);
		static std::array<TerrainChunkRegion, ChunkCount> Build(
			float terrainWorldSize,
			uint32_t sharedMeshResolution);
	};

}
