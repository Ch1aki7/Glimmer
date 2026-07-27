#pragma once

#include "Glimmer/Core/Core.h"
#include "Glimmer/Renderer/TerrainMesh.h"
#include "Glimmer/Terrain/TerrainGenerator.h"

namespace gl {
	struct TerrainRuntime
	{
		Scope<TerrainGenerator> Generator;
		Ref<TerrainMesh> Mesh;
		Ref<Texture2D> HeightMap;
		AssetHandle LoadedHeightMapHandle{ 0 };
		AssetHandle LoadedGenerationShaderHandle{ 0 };
		uint32_t LoadedHeightMapResolution = 0;
		uint32_t LoadedMeshResolution = 0;
		bool Dirty = true;
	};
}