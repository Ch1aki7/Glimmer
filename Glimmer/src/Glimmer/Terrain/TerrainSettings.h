#pragma once

#include "Glimmer/Asset/Asset.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace gl {
	struct TerrainNoiseSettings
	{
		int Seed = 1;
		int Octaves = 7;
		float Frequency = 2.2f;
		float Lacunarity = 2.0f;
		float Persistence = 0.48f;
		float DomainWarp = 0.65f;
		float RidgeStrength = 0.58f;
		float ContinentScale = 0.32f;
		float ErosionStrength = 0.18f;
		float DetailStrength = 0.07f;
		glm::vec2 Offset{ 0.0f };
	};

	struct TerrainSpecification
	{
		bool Procedural = true;
		uint32_t HeightMapResolution = 512;
		uint32_t MeshResolution = 256;
		float HeightScale = 24.0f;
		AssetHandle HeightMapHandle{ 0 };
		AssetHandle RenderShaderHandle{ 0 };
		AssetHandle GenerationShaderHandle{ 0 };
		TerrainNoiseSettings Noise;
	};
}