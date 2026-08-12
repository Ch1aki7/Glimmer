#pragma once

#include "Glimmer/Core/Core.h"
#include "Glimmer/Renderer/TerrainMesh.h"
#include "Glimmer/Terrain/TerrainGenerator.h"

#include <array>

namespace gl {
	struct TerrainRuntime
	{
		Scope<TerrainGenerator> Generator;
		Ref<TerrainMesh> Mesh;
		std::array<Ref<TerrainMesh>, 3> LODMeshes;
		std::array<uint32_t, 9> ChunkLODLevels{};
		bool HasChunkLODHistory = false;
		Ref<Texture2D> HeightMap;
		Ref<Texture2D> NormalSlopeMap;
		Ref<Texture2D> AnalysisMap;
		Ref<Texture2D> MaterialWeightMap;
		AssetHandle LoadedHeightMapHandle{ 0 };
		AssetHandle LoadedGenerationShaderHandle{ 0 };
		AssetHandle LoadedErosionShaderHandle{ 0 };
		AssetHandle LoadedDerivationShaderHandle{ 0 };
		uint32_t LoadedHeightMapResolution = 0;
		uint32_t LoadedMeshResolution = 0;
		uint32_t LastGenerationDispatchCount = 0;
		uint64_t GenerationVersion = 0;
		bool ValidationComplete = false;
		bool Dirty = true;
	};
}
