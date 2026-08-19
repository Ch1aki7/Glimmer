#pragma once

#include <Glimmer/Core/Core.h>

#include <glm/glm.hpp>
#include <vector>

namespace gl {

	struct TerrainClimateSpecification
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		float CellSize = 1.0f;
		float FixedTimeStep = 1.0f / 60.0f;
		uint32_t MaxSubsteps = 8;
		glm::vec2 WindVelocity = glm::vec2(1.0f, 0.0f);
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
	};

	struct TerrainClimateState
	{
		// Metres above the simulation datum; immutable during a climate step.
		std::vector<float> TerrainHeight;
		// Degrees Celsius.
		std::vector<float> Temperature;
		// Metres of water-equivalent depth in the local atmospheric column.
		std::vector<float> AtmosphericMoisture;
		// Metres of liquid water available for evaporation and vegetation.
		std::vector<float> SurfaceWater;
		// Metres condensed during the most recent fixed step.
		std::vector<float> Rainfall;
		// Dimensionless suitability in the closed range [0, 1].
		std::vector<float> VegetationPotential;
	};

	struct TerrainClimateStatistics
	{
		uint64_t StepCount = 0;
		double SimulatedTime = 0.0;
		double Accumulator = 0.0;
		double DroppedTime = 0.0;
		double InitialWaterVolume = 0.0;
		double AtmosphericWaterVolume = 0.0;
		double SurfaceWaterVolume = 0.0;
		double CumulativeEvaporationVolume = 0.0;
		double CumulativeRainfallVolume = 0.0;
		double WaterBudgetError = 0.0;
		float MinimumTemperature = 0.0f;
		float MaximumTemperature = 0.0f;
		float MinimumAtmosphericMoisture = 0.0f;
		float MaximumAtmosphericMoisture = 0.0f;
		float MaximumRainfall = 0.0f;
		float MinimumVegetationPotential = 0.0f;
		float MaximumVegetationPotential = 0.0f;
		bool Finite = true;
	};

	class TerrainClimateRuntime
	{
	public:
		TerrainClimateRuntime(
			const TerrainClimateSpecification& specification,
			const std::vector<float>& terrainHeight);

		void Play() { m_Playing = true; }
		void Pause() { m_Playing = false; }
		bool IsPlaying() const { return m_Playing; }
		uint32_t Advance(float frameDeltaSeconds);
		bool SingleStep();
		void Reset();
		void SetTemperature(const std::vector<float>& temperature);
		void SetAtmosphericMoisture(const std::vector<float>& moistureDepth);
		void SetSurfaceWater(const std::vector<float>& waterDepth);
		void SetVegetationPotential(const std::vector<float>& potential);

		const TerrainClimateSpecification& GetSpecification() const
		{
			return m_Specification;
		}
		const TerrainClimateState& GetState() const { return m_State; }
		const TerrainClimateStatistics& GetStatistics() const
		{
			return m_Statistics;
		}

	private:
		void Step(float deltaSeconds);
		void AdvectAtmosphericMoisture(
			const std::vector<float>& source, std::vector<float>& destination,
			float deltaSeconds) const;
		float CalculateSaturationMoisture(float temperature) const;
		glm::vec2 CalculateTerrainGradient(uint32_t x, uint32_t y) const;
		void UpdateStatistics();
		size_t Index(uint32_t x, uint32_t y) const;

		TerrainClimateSpecification m_Specification;
		TerrainClimateState m_State;
		TerrainClimateState m_InitialState;
		TerrainClimateState m_NextState;
		TerrainClimateStatistics m_Statistics;
		bool m_Playing = false;
		double m_Accumulator = 0.0;
	};

}
