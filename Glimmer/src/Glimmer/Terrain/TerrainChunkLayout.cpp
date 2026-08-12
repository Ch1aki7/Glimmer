#include "glpch.h"
#include "TerrainChunkLayout.h"

namespace gl {

	uint32_t TerrainChunkLayout::CalculateSharedMeshResolution(
		uint32_t terrainMeshResolution)
	{
		const uint32_t sanitized = glm::max(terrainMeshResolution, 1u);
		return (sanitized + AxisCount - 1) / AxisCount;
	}

	std::array<TerrainChunkRegion, TerrainChunkLayout::ChunkCount>
		TerrainChunkLayout::Build(
			float terrainWorldSize,
			uint32_t sharedMeshResolution)
	{
		std::array<TerrainChunkRegion, ChunkCount> chunks;
		const float worldSize = glm::max(terrainWorldSize, 0.0001f);
		const float chunkWorldSize = worldSize / static_cast<float>(AxisCount);
		const float uvScale = 1.0f / static_cast<float>(AxisCount);
		const float localScale = chunkWorldSize
			/ static_cast<float>(glm::max(sharedMeshResolution, 1u));
		const float minimumCenter = -worldSize * 0.5f
			+ chunkWorldSize * 0.5f;

		for (uint32_t z = 0; z < AxisCount; ++z)
		{
			for (uint32_t x = 0; x < AxisCount; ++x)
			{
				TerrainChunkRegion& chunk = chunks[z * AxisCount + x];
				chunk.Coordinate = { x, z };
				chunk.UVOffset = {
					static_cast<float>(x) * uvScale,
					static_cast<float>(z) * uvScale
				};
				chunk.UVScale = { uvScale, uvScale };
				chunk.LocalOffset = {
					minimumCenter + static_cast<float>(x) * chunkWorldSize,
					minimumCenter + static_cast<float>(z) * chunkWorldSize
				};
				chunk.LocalScale = localScale;
				chunk.WorldSize = chunkWorldSize;
			}
		}
		return chunks;
	}

}
