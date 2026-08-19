#pragma once

#include "Glimmer/Renderer/ComputeShader.h"
#include "Glimmer/Simulation/SimulationGrid.h"

#include <filesystem>
#include <vector>

namespace gl {

	struct TerrainClimateGPUSettings
	{
		float FixedTimeStep = 1.0f / 60.0f;
		uint32_t MaxSubsteps = 4;
		glm::vec2 WindVelocity = { 1.0f, 0.0f };
		float SeaLevelTemperature = 20.0f;
		float TemperatureLapseRate = 0.0065f;
		float TemperatureRelaxationRate = 0.1f;
		float SaturationReferenceTemperature = 20.0f;
		float SaturationMoistureDepth = 0.02f;
		float SaturationTemperatureSensitivity = 0.04f;
		float EvaporationRate = 0.0005f;
		float CondensationRate = 1.0f;
		float OrographicRainRate = 0.25f;
		float VegetationResponseRate = 0.2f;
		float VegetationOptimalTemperature = 18.0f;
		float VegetationTemperatureRange = 20.0f;
		float VegetationMoistureForFullCover = 0.02f;
		float InitialAtmosphericMoisture = 0.01f;
	};

	struct TerrainClimateGPUStatistics
	{
		uint64_t StepCount = 0;
		double SimulatedTime = 0.0;
		double Accumulator = 0.0;
		double DroppedTime = 0.0;
		double AtmosphericWaterVolume = 0.0;
		double SurfaceWaterVolume = 0.0;
		double RainfallVolume = 0.0;
		float MinimumTemperature = 0.0f;
		float MaximumTemperature = 0.0f;
		float MinimumAtmosphericMoisture = 0.0f;
		float MaximumAtmosphericMoisture = 0.0f;
		float MaximumRainfall = 0.0f;
		float MinimumVegetationPotential = 0.0f;
		float MaximumVegetationPotential = 0.0f;
		bool Finite = true;
		bool ReadbackAvailable = false;
	};

	struct TerrainClimateGPUValidationResult
	{
		bool Attempted = false;
		bool Passed = false;
		bool Finite = false;
		bool WindTransportValid = false;
		bool OrographicRainValid = false;
		bool FramePartitionIndependent = false;
		float DownwindMoisture = 0.0f;
		float RisingTerrainRainfall = 0.0f;
		float FlatTerrainRainfall = 0.0f;
		float MaximumPartitionDifference = 0.0f;
		std::string Message;
	};

	class TerrainClimateGPU
	{
	public:
		TerrainClimateGPU(uint32_t width, uint32_t height,
			std::filesystem::path sourceShaderPath,
			std::filesystem::path advectionShaderPath,
			std::filesystem::path responseShaderPath);

		uint32_t Advance(float frameDeltaSeconds,
			const Ref<Texture2D>& heightMap,
			const Ref<Texture2D>& surfaceWater,
			float heightScale, float worldSize);
		void SingleStep(const Ref<Texture2D>& heightMap,
			const Ref<Texture2D>& surfaceWater,
			float heightScale, float worldSize);
		void Reset();
		void SetAtmosphericMoisture(const std::vector<float>& moistureDepth);
		void ReadbackStatistics(
			const Ref<Texture2D>& surfaceWater, float worldSize);
		bool ReloadShadersIfChanged();
		static TerrainClimateGPUValidationResult ValidateContract(
			const std::filesystem::path& sourceShaderPath,
			const std::filesystem::path& advectionShaderPath,
			const std::filesystem::path& responseShaderPath);

		const Ref<Texture2D>& GetTemperatureTexture() const
		{
			return m_Temperature.ReadTexture();
		}
		const Ref<Texture2D>& GetAtmosphericMoistureTexture() const
		{
			return m_AtmosphericMoisture.ReadTexture();
		}
		const Ref<Texture2D>& GetRainfallTexture() const
		{
			return m_Rainfall;
		}
		const Ref<Texture2D>& GetVegetationPotentialTexture() const
		{
			return m_VegetationPotential.ReadTexture();
		}
		TerrainClimateGPUSettings& GetSettings() { return m_Settings; }
		const TerrainClimateGPUStatistics& GetStatistics() const
		{
			return m_Statistics;
		}

	private:
		void Step(const Ref<Texture2D>& heightMap,
			const Ref<Texture2D>& surfaceWater,
			float heightScale, float worldSize);
		const Ref<Texture2D>& ResolveSurfaceWater(
			const Ref<Texture2D>& surfaceWater) const;
		void Dispatch(const Ref<ComputeShader>& shader) const;

		SimulationGrid m_Temperature;
		SimulationGrid m_AtmosphericMoisture;
		SimulationGrid m_VegetationPotential;
		Ref<Texture2D> m_Rainfall;
		Ref<Texture2D> m_ZeroSurfaceWater;
		Ref<ComputeShader> m_SourceShader;
		Ref<ComputeShader> m_AdvectionShader;
		Ref<ComputeShader> m_ResponseShader;
		TerrainClimateGPUSettings m_Settings;
		TerrainClimateGPUStatistics m_Statistics;
		double m_Accumulator = 0.0;
	};

}
