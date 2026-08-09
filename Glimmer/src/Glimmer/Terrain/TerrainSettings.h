#pragma once

#include "Glimmer/Asset/Asset.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <string>

namespace gl {
	enum class TerrainPreset
	{
		Custom = 0,
		Alpine,
		Plateau,
		RollingHills,
		Volcanic,
		ErodedValley
	};

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
		float MountainDirection = 0.35f;
		float MountainWidth = 0.32f;
		float PlateauStrength = 0.0f;
		glm::vec2 Offset{ 0.0f };
	};

	struct TerrainAuthoringSettings
	{
		bool EnableThermalErosion = true;
		uint32_t ThermalIterations = 24;
		float Talus = 0.012f;
		float ThermalStrength = 0.35f;
	};

	struct TerrainSpecification
	{
		bool Procedural = true;
		TerrainPreset Preset = TerrainPreset::Custom;
		uint32_t HeightMapResolution = 512;
		uint32_t MeshResolution = 256;
		float HeightScale = 24.0f;
		AssetHandle HeightMapHandle{ 0 };
		AssetHandle RenderShaderHandle{ 0 };
		AssetHandle GenerationShaderHandle{ 0 };
		AssetHandle ErosionShaderHandle{ 0 };
		AssetHandle DerivationShaderHandle{ 0 };
		TerrainNoiseSettings Noise;
		TerrainAuthoringSettings Authoring;
	};

	const char* TerrainPresetToString(TerrainPreset preset);
	TerrainPreset TerrainPresetFromString(const std::string& value);
	void ApplyTerrainPreset(TerrainSpecification& specification, TerrainPreset preset);
}
