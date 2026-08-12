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
		static std::array<uint32_t, 3> CalculateLODResolutions(
			uint32_t highestResolution);
		static uint32_t SelectLODLevel(float distance,
			float middleDistance, float farDistance);
		static uint32_t SelectLODLevelWithHysteresis(float distance,
			float middleDistance, float farDistance, uint32_t previousLevel,
			float hysteresis);
		static std::array<uint32_t, ChunkCount> StabilizeNeighborLODs(
			std::array<uint32_t, ChunkCount> levels);
	};

}
