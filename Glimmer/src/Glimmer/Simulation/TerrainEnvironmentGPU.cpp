#include "glpch.h"
#include "Glimmer/Simulation/TerrainEnvironmentGPU.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gl {

	TerrainEnvironmentGPU::TerrainEnvironmentGPU(
		uint32_t width, uint32_t height)
		: m_Width(std::max(width, 1u)), m_Height(std::max(height, 1u))
	{
	}

	uint32_t TerrainEnvironmentGPU::Advance(float frameDeltaSeconds,
		TerrainClimateGPU& climate, TerrainHydrologyGPU& hydrology,
		float heightScale, float worldSize)
	{
		if (!std::isfinite(frameDeltaSeconds) || frameDeltaSeconds <= 0.0f)
			return 0;

		m_Accumulator += frameDeltaSeconds;
		const double fixedTimeStep = std::max(
			static_cast<double>(m_Settings.FixedTimeStep), 1.0e-6);
		const uint32_t maxSubsteps = std::max(m_Settings.MaxSubsteps, 1u);
		uint32_t steps = 0;
		while (m_Accumulator + std::numeric_limits<double>::epsilon()
			>= fixedTimeStep && steps < maxSubsteps)
		{
			Step(climate, hydrology, heightScale, worldSize);
			m_Accumulator -= fixedTimeStep;
			++steps;
		}
		if (steps == maxSubsteps && m_Accumulator >= fixedTimeStep)
		{
			const double retained = std::fmod(m_Accumulator, fixedTimeStep);
			m_Statistics.DroppedTime += m_Accumulator - retained;
			m_Accumulator = retained;
		}
		m_Statistics.Accumulator = m_Accumulator;
		return steps;
	}

	void TerrainEnvironmentGPU::SingleStep(TerrainClimateGPU& climate,
		TerrainHydrologyGPU& hydrology, float heightScale, float worldSize)
	{
		Step(climate, hydrology, heightScale, worldSize);
	}

	void TerrainEnvironmentGPU::Reset(TerrainClimateGPU& climate,
		TerrainHydrologyGPU& hydrology, float worldSize)
	{
		climate.Reset();
		hydrology.Reset();
		m_Accumulator = 0.0;
		m_Statistics = {};
		m_Statistics.InitialTotalWaterVolume =
			static_cast<double>(std::max(
				climate.GetSettings().InitialAtmosphericMoisture, 0.0f))
			* GridArea(worldSize);
		m_Statistics.ExpectedTotalWaterVolume =
			m_Statistics.InitialTotalWaterVolume;
	}

	void TerrainEnvironmentGPU::ReadbackStatistics(TerrainClimateGPU& climate,
		TerrainHydrologyGPU& hydrology, float worldSize, float heightScale)
	{
		climate.ReadbackStatistics(hydrology.GetWaterTexture(), worldSize);
		hydrology.ReadbackStatistics(worldSize, heightScale);
		const auto& climateStatistics = climate.GetStatistics();
		const auto& hydrologyStatistics = hydrology.GetStatistics();
		m_Statistics.TotalWaterVolume =
			climateStatistics.AtmosphericWaterVolume
			+ hydrologyStatistics.WaterVolume;
		m_Statistics.ExpectedTotalWaterVolume =
			m_Statistics.InitialTotalWaterVolume
			+ m_Statistics.ExternalWaterVolume;
		m_Statistics.MassError = m_Statistics.TotalWaterVolume
			- m_Statistics.ExpectedTotalWaterVolume;
		m_Statistics.Finite = climateStatistics.Finite
			&& hydrologyStatistics.Finite
			&& std::isfinite(m_Statistics.TotalWaterVolume)
			&& std::isfinite(m_Statistics.MassError);
		m_Statistics.ReadbackAvailable = true;
	}

	void TerrainEnvironmentGPU::Step(TerrainClimateGPU& climate,
		TerrainHydrologyGPU& hydrology, float heightScale, float worldSize)
	{
		const float deltaSeconds = std::max(m_Settings.FixedTimeStep, 1.0e-6f);
		climate.GetSettings().FixedTimeStep = deltaSeconds;
		hydrology.GetSettings().FixedTimeStep = deltaSeconds;

		climate.SingleStep(hydrology.GetHeightTexture(),
			hydrology.GetWaterTexture(), heightScale, worldSize);
		// TerrainClimateGPU ends its WaterSource pass with a global image barrier.
		hydrology.SingleStep(hydrology.GetHeightTexture(), heightScale, worldSize,
			climate.GetWaterSourceTexture());

		m_Statistics.ExternalWaterVolume += static_cast<double>(
			std::max(hydrology.GetSettings().RainfallRate, 0.0f))
			* deltaSeconds * GridArea(worldSize);
		++m_Statistics.StepCount;
		m_Statistics.SimulatedTime += deltaSeconds;
		m_Statistics.ReadbackAvailable = false;
	}

	double TerrainEnvironmentGPU::GridArea(float worldSize) const
	{
		const double cellSize = static_cast<double>(
			std::max(worldSize, 0.0001f)) / std::max(m_Width - 1u, 1u);
		return cellSize * cellSize * m_Width * m_Height;
	}

}
