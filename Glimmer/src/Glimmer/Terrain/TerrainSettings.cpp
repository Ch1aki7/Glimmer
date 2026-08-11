#include "glpch.h"
#include "TerrainSettings.h"

namespace gl {
	const char* TerrainPresetToString(TerrainPreset preset)
	{
		switch (preset)
		{
			case TerrainPreset::Alpine: return "Alpine";
			case TerrainPreset::Plateau: return "Plateau";
			case TerrainPreset::RollingHills: return "Rolling Hills";
			case TerrainPreset::Volcanic: return "Volcanic";
			case TerrainPreset::ErodedValley: return "Eroded Valley";
			default: return "Custom";
		}
	}

	TerrainPreset TerrainPresetFromString(const std::string& value)
	{
		if (value == "Alpine") return TerrainPreset::Alpine;
		if (value == "Plateau") return TerrainPreset::Plateau;
		if (value == "Rolling Hills") return TerrainPreset::RollingHills;
		if (value == "Volcanic") return TerrainPreset::Volcanic;
		if (value == "Eroded Valley") return TerrainPreset::ErodedValley;
		return TerrainPreset::Custom;
	}

	void ApplyTerrainPreset(TerrainSpecification& specification, TerrainPreset preset)
	{
		if (preset == TerrainPreset::Custom)
		{
			specification.Preset = preset;
			return;
		}

		TerrainNoiseSettings noise;
		TerrainAuthoringSettings authoring;
		float heightScale = 24.0f;

		switch (preset)
		{
			case TerrainPreset::Alpine:
				noise.Seed = 11;
				noise.Frequency = 2.4f;
				noise.DomainWarp = 0.82f;
				noise.RidgeStrength = 0.82f;
				noise.ContinentScale = 0.30f;
				noise.ErosionStrength = 0.16f;
				noise.DetailStrength = 0.08f;
				noise.MountainDirection = 0.35f;
				noise.MountainWidth = 0.24f;
				noise.GeologyBlend = 0.72f;
				noise.GeologyScale = 3.4f;
				noise.RiftStrength = 0.08f;
				noise.TrendStrength = 0.12f;
				heightScale = 42.0f;
				authoring.ThermalIterations = 28;
				authoring.Talus = 0.010f;
				authoring.ThermalStrength = 0.30f;
				break;
			case TerrainPreset::Plateau:
				noise.Seed = 23;
				noise.Octaves = 6;
				noise.Frequency = 1.65f;
				noise.DomainWarp = 0.35f;
				noise.RidgeStrength = 0.32f;
				noise.ContinentScale = 0.46f;
				noise.ErosionStrength = 0.10f;
				noise.DetailStrength = 0.035f;
				noise.PlateauStrength = 0.82f;
				noise.GeologyBlend = 0.42f;
				noise.GeologyScale = 2.2f;
				noise.RiftStrength = 0.14f;
				noise.TrendStrength = 0.10f;
				heightScale = 30.0f;
				authoring.ThermalIterations = 14;
				authoring.Talus = 0.018f;
				authoring.ThermalStrength = 0.22f;
				break;
			case TerrainPreset::RollingHills:
				noise.Seed = 37;
				noise.Octaves = 5;
				noise.Frequency = 1.35f;
				noise.Lacunarity = 1.85f;
				noise.Persistence = 0.42f;
				noise.DomainWarp = 0.28f;
				noise.RidgeStrength = 0.12f;
				noise.ContinentScale = 0.55f;
				noise.ErosionStrength = 0.035f;
				noise.DetailStrength = 0.025f;
				noise.GeologyBlend = 0.16f;
				noise.GeologyScale = 1.6f;
				noise.RiftStrength = 0.02f;
				noise.TrendStrength = 0.08f;
				heightScale = 16.0f;
				authoring.ThermalIterations = 8;
				authoring.Talus = 0.020f;
				authoring.ThermalStrength = 0.18f;
				break;
			case TerrainPreset::Volcanic:
				noise.Seed = 53;
				noise.Octaves = 7;
				noise.Frequency = 2.0f;
				noise.DomainWarp = 0.48f;
				noise.RidgeStrength = 0.55f;
				noise.ContinentScale = 0.38f;
				noise.ErosionStrength = 0.08f;
				noise.DetailStrength = 0.06f;
				noise.GeologyBlend = 0.12f;
				noise.GeologyScale = 2.5f;
				noise.RiftStrength = 0.03f;
				noise.TrendStrength = 0.04f;
				heightScale = 38.0f;
				authoring.ThermalIterations = 18;
				authoring.Talus = 0.014f;
				authoring.ThermalStrength = 0.26f;
				break;
			case TerrainPreset::ErodedValley:
				noise.Seed = 71;
				noise.Octaves = 8;
				noise.Frequency = 2.15f;
				noise.DomainWarp = 1.05f;
				noise.RidgeStrength = 0.67f;
				noise.ContinentScale = 0.34f;
				noise.ErosionStrength = 0.34f;
				noise.DetailStrength = 0.045f;
				noise.MountainDirection = -0.55f;
				noise.MountainWidth = 0.30f;
				noise.GeologyBlend = 0.68f;
				noise.GeologyScale = 2.8f;
				noise.RiftStrength = 0.22f;
				noise.TrendStrength = 0.14f;
				heightScale = 34.0f;
				authoring.ThermalIterations = 48;
				authoring.Talus = 0.008f;
				authoring.ThermalStrength = 0.38f;
				break;
			default:
				break;
		}

		specification.Preset = preset;
		specification.Noise = noise;
		specification.Authoring = authoring;
		specification.HeightScale = heightScale;
	}
}
