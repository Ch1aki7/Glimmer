#pragma once

#include "Glimmer/Simulation/TerrainClimateGPU.h"
#include "Glimmer/Simulation/TerrainHydrologyGPU.h"

namespace gl {

	struct TerrainEnvironmentGPUSettings
	{
		float FixedTimeStep = 1.0f / 60.0f;
		uint32_t MaxSubsteps = 4;
	};

	struct TerrainEnvironmentGPUStatistics
	{
		uint64_t StepCount = 0;
		double SimulatedTime = 0.0;
		double Accumulator = 0.0;
		double DroppedTime = 0.0;
		double InitialTotalWaterVolume = 0.0;
		double ExternalWaterVolume = 0.0;
		double ExpectedTotalWaterVolume = 0.0;
		double TotalWaterVolume = 0.0;
		double MassError = 0.0;
		bool Finite = true;
		bool ReadbackAvailable = false;
	};

	class TerrainEnvironmentGPU
	{
	public:
		TerrainEnvironmentGPU(uint32_t width, uint32_t height);

		uint32_t Advance(float frameDeltaSeconds,
			TerrainClimateGPU& climate, TerrainHydrologyGPU& hydrology,
			float heightScale, float worldSize);
		void SingleStep(TerrainClimateGPU& climate,
			TerrainHydrologyGPU& hydrology, float heightScale, float worldSize);
		void Reset(TerrainClimateGPU& climate, TerrainHydrologyGPU& hydrology,
			float worldSize);
		void ReadbackStatistics(TerrainClimateGPU& climate,
			TerrainHydrologyGPU& hydrology, float worldSize, float heightScale);

		TerrainEnvironmentGPUSettings& GetSettings() { return m_Settings; }
		const TerrainEnvironmentGPUStatistics& GetStatistics() const
		{
			return m_Statistics;
		}

	private:
		void Step(TerrainClimateGPU& climate, TerrainHydrologyGPU& hydrology,
			float heightScale, float worldSize);
		double GridArea(float worldSize) const;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		TerrainEnvironmentGPUSettings m_Settings;
		TerrainEnvironmentGPUStatistics m_Statistics;
		double m_Accumulator = 0.0;
	};

}
