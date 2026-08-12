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

	std::array<uint32_t, 3> TerrainChunkLayout::CalculateLODResolutions(
		uint32_t highestResolution)
	{
		const uint32_t high = glm::max(highestResolution, 1u);
		return {
			high,
			glm::max((high + 1u) / 2u, 1u),
			glm::max((high + 3u) / 4u, 1u)
		};
	}

	uint32_t TerrainChunkLayout::SelectLODLevel(float distance,
		float middleDistance, float farDistance)
	{
		const float middle = glm::max(middleDistance, 0.0f);
		const float farThreshold = glm::max(farDistance, middle);
		if (distance >= farThreshold)
			return 2;
		if (distance >= middle)
			return 1;
		return 0;
	}

	uint32_t TerrainChunkLayout::SelectLODLevelWithHysteresis(float distance,
		float middleDistance, float farDistance, uint32_t previousLevel,
		float hysteresis)
	{
		const float middle = glm::max(middleDistance, 0.0f);
		const float farThreshold = glm::max(farDistance, middle);
		const float band = glm::max(hysteresis, 0.0f);
		switch (glm::min(previousLevel, 2u))
		{
		case 0:
			if (distance >= farThreshold + band) return 2;
			return distance >= middle + band ? 1u : 0u;
		case 1:
			if (distance < middle - band) return 0;
			return distance >= farThreshold + band ? 2u : 1u;
		default:
			if (distance < middle - band) return 0;
			return distance < farThreshold - band ? 1u : 2u;
		}
	}

	std::array<uint32_t, TerrainChunkLayout::ChunkCount>
		TerrainChunkLayout::StabilizeNeighborLODs(
			std::array<uint32_t, ChunkCount> levels)
	{
		for (uint32_t& level : levels)
			level = glm::min(level, 2u);
		bool changed = true;
		while (changed)
		{
			changed = false;
			for (uint32_t z = 0; z < AxisCount; ++z)
			{
				for (uint32_t x = 0; x < AxisCount; ++x)
				{
					const uint32_t index = z * AxisCount + x;
					const auto constrain = [&](uint32_t neighbor) {
						if (levels[index] > levels[neighbor] + 1u)
						{
							levels[index] = levels[neighbor] + 1u;
							changed = true;
						}
					};
					if (x > 0) constrain(index - 1);
					if (x + 1 < AxisCount) constrain(index + 1);
					if (z > 0) constrain(index - AxisCount);
					if (z + 1 < AxisCount) constrain(index + AxisCount);
				}
			}
		}
		return levels;
	}

}
