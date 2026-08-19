#include <glpch.h>
#include <Glimmer/Simulation/TerrainClimateRuntime.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace gl {
	namespace {
		bool IsFiniteNonNegative(float value)
		{
			return std::isfinite(value) && value >= 0.0f;
		}

		bool IsFiniteUnit(float value)
		{
			return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
		}

		bool IsValidSpecification(const TerrainClimateSpecification& value)
		{
			if (value.Width == 0 || value.Height == 0 || value.MaxSubsteps == 0)
				return false;
			if (!IsFiniteNonNegative(value.CellSize) || value.CellSize == 0.0f)
				return false;
			if (!IsFiniteNonNegative(value.FixedTimeStep)
				|| value.FixedTimeStep == 0.0f)
				return false;
			if (!std::isfinite(value.WindVelocity.x)
				|| !std::isfinite(value.WindVelocity.y))
				return false;
			if (!std::isfinite(value.SeaLevelTemperature)
				|| !std::isfinite(value.SaturationReferenceTemperature)
				|| !std::isfinite(value.VegetationOptimalTemperature))
				return false;
			if (!IsFiniteNonNegative(value.TemperatureLapseRate)
				|| !IsFiniteNonNegative(value.TemperatureRelaxationRate)
				|| !IsFiniteNonNegative(value.SaturationMoistureDepth)
				|| !IsFiniteNonNegative(value.SaturationTemperatureSensitivity))
				return false;
			if (!IsFiniteNonNegative(value.EvaporationRate)
				|| !IsFiniteNonNegative(value.CondensationRate)
				|| !IsFiniteNonNegative(value.OrographicRainRate)
				|| !IsFiniteNonNegative(value.VegetationResponseRate))
				return false;
			if (!IsFiniteNonNegative(value.VegetationTemperatureRange)
				|| value.VegetationTemperatureRange == 0.0f)
				return false;
			return IsFiniteNonNegative(value.VegetationMoistureForFullCover)
				&& value.VegetationMoistureForFullCover > 0.0f;
		}
	}

	TerrainClimateRuntime::TerrainClimateRuntime(
		const TerrainClimateSpecification& specification,
		const std::vector<float>& terrainHeight)
		: m_Specification(specification)
	{
		if (!IsValidSpecification(specification))
			throw std::invalid_argument(std::string{});
		const size_t cellCount = static_cast<size_t>(specification.Width)
			* specification.Height;
		const bool validHeight = terrainHeight.size() == cellCount
			&& std::all_of(terrainHeight.begin(), terrainHeight.end(),
				[](float value) { return std::isfinite(value); });
		if (!validHeight)
			throw std::invalid_argument(std::string{});
		m_InitialState.TerrainHeight = terrainHeight;
		m_InitialState.Temperature.resize(cellCount);
		for (size_t index = 0; index < cellCount; ++index)
		{
			m_InitialState.Temperature[index] =
				specification.SeaLevelTemperature
				- specification.TemperatureLapseRate * terrainHeight[index];
		}
		m_InitialState.AtmosphericMoisture.assign(cellCount, 0.0f);
		m_InitialState.SurfaceWater.assign(cellCount, 0.0f);
		m_InitialState.Rainfall.assign(cellCount, 0.0f);
		m_InitialState.VegetationPotential.assign(cellCount, 0.0f);
		Reset();
	}

	uint32_t TerrainClimateRuntime::Advance(float frameDeltaSeconds)
	{
		if (!m_Playing || !std::isfinite(frameDeltaSeconds)
			|| frameDeltaSeconds <= 0.0f)
			return 0;

		m_Accumulator += static_cast<double>(frameDeltaSeconds);
		const double fixedTimeStep = m_Specification.FixedTimeStep;
		uint32_t substeps = 0;
		while (m_Accumulator + std::numeric_limits<double>::epsilon()
			>= fixedTimeStep && substeps < m_Specification.MaxSubsteps)
		{
			Step(m_Specification.FixedTimeStep);
			m_Accumulator -= fixedTimeStep;
			++substeps;
		}
		if (substeps == m_Specification.MaxSubsteps
			&& m_Accumulator >= fixedTimeStep)
		{
			const double retained = std::fmod(m_Accumulator, fixedTimeStep);
			m_Statistics.DroppedTime += m_Accumulator - retained;
			m_Accumulator = retained;
		}
		m_Statistics.Accumulator = m_Accumulator;
		return substeps;
	}

	bool TerrainClimateRuntime::SingleStep()
	{
		if (m_Playing)
			return false;
		Step(m_Specification.FixedTimeStep);
		return true;
	}

	void TerrainClimateRuntime::Reset()
	{
		m_Playing = false;
		m_Accumulator = 0.0;
		m_State = m_InitialState;
		std::fill(m_State.Rainfall.begin(), m_State.Rainfall.end(), 0.0f);
		m_NextState = m_State;
		m_Statistics = {};
		UpdateStatistics();
		m_Statistics.InitialWaterVolume =
			m_Statistics.AtmosphericWaterVolume
			+ m_Statistics.SurfaceWaterVolume;
		m_Statistics.WaterBudgetError = 0.0;
	}

	void TerrainClimateRuntime::Step(float deltaSeconds)
	{
		const size_t cellCount = m_State.TerrainHeight.size();
		m_NextState = m_State;
		std::fill(m_NextState.Rainfall.begin(), m_NextState.Rainfall.end(), 0.0f);
		const float temperatureBlend = glm::clamp(
			m_Specification.TemperatureRelaxationRate * deltaSeconds,
			0.0f, 1.0f);
		double evaporatedVolume = 0.0;
		const double cellArea = static_cast<double>(m_Specification.CellSize)
			* m_Specification.CellSize;

		for (size_t index = 0; index < cellCount; ++index)
		{
			const float targetTemperature =
				m_Specification.SeaLevelTemperature
				- m_Specification.TemperatureLapseRate
					* m_State.TerrainHeight[index];
			const float temperature = glm::mix(
				m_State.Temperature[index], targetTemperature, temperatureBlend);
			m_NextState.Temperature[index] = temperature;

			const float saturation = CalculateSaturationMoisture(temperature);
			const float relativeHumidity = saturation > 1.0e-8f
				? glm::clamp(m_State.AtmosphericMoisture[index] / saturation,
					0.0f, 1.0f)
				: 1.0f;
			const float temperatureFactor = glm::clamp(
				(temperature + 5.0f) / 25.0f, 0.0f, 2.0f);
			const float evaporation = std::min(
				m_State.SurfaceWater[index],
				m_Specification.EvaporationRate * temperatureFactor
					* (1.0f - relativeHumidity) * deltaSeconds);
			m_NextState.SurfaceWater[index] =
				m_State.SurfaceWater[index] - evaporation;
			m_NextState.AtmosphericMoisture[index] =
				m_State.AtmosphericMoisture[index] + evaporation;
			evaporatedVolume += static_cast<double>(evaporation) * cellArea;
		}

		std::vector<float> advectedMoisture;
		AdvectAtmosphericMoisture(
			m_NextState.AtmosphericMoisture, advectedMoisture, deltaSeconds);
		m_NextState.AtmosphericMoisture = std::move(advectedMoisture);
		m_Statistics.CumulativeEvaporationVolume += evaporatedVolume;

		double rainfallVolume = 0.0;
		for (uint32_t y = 0; y < m_Specification.Height; ++y)
		{
			for (uint32_t x = 0; x < m_Specification.Width; ++x)
			{
				const size_t index = Index(x, y);
				float moisture = m_NextState.AtmosphericMoisture[index];
				const float saturation = CalculateSaturationMoisture(
					m_NextState.Temperature[index]);
				const float saturationExcess = std::max(moisture - saturation, 0.0f);
				const float condensationFraction = glm::clamp(
					m_Specification.CondensationRate * deltaSeconds, 0.0f, 1.0f);
				float rainfall = saturationExcess * condensationFraction;

				const glm::vec2 gradient = CalculateTerrainGradient(x, y);
				const float upwardVelocity = std::max(
					glm::dot(m_Specification.WindVelocity, gradient), 0.0f);
				const float liftFraction = glm::clamp(
					m_Specification.OrographicRainRate * upwardVelocity
						* deltaSeconds, 0.0f, 1.0f);
				rainfall += (moisture - rainfall) * liftFraction;
				rainfall = std::min(rainfall, moisture);
				moisture -= rainfall;
				m_NextState.AtmosphericMoisture[index] = std::max(moisture, 0.0f);
				m_NextState.SurfaceWater[index] += rainfall;
				m_NextState.Rainfall[index] = rainfall;
				rainfallVolume += static_cast<double>(rainfall) * cellArea;

				const float moistureSuitability = glm::clamp(
					m_NextState.SurfaceWater[index]
						/ m_Specification.VegetationMoistureForFullCover,
					0.0f, 1.0f);
				const float temperatureSuitability = glm::clamp(
					1.0f - std::abs(m_NextState.Temperature[index]
						- m_Specification.VegetationOptimalTemperature)
						/ m_Specification.VegetationTemperatureRange,
					0.0f, 1.0f);
				const float targetVegetation =
					moistureSuitability * temperatureSuitability;
				const float vegetationBlend = glm::clamp(
					m_Specification.VegetationResponseRate * deltaSeconds,
					0.0f, 1.0f);
				m_NextState.VegetationPotential[index] = glm::mix(
					m_State.VegetationPotential[index],
					targetVegetation, vegetationBlend);
			}
		}

		m_Statistics.CumulativeRainfallVolume += rainfallVolume;
		m_State = std::move(m_NextState);
		m_NextState = m_State;
		++m_Statistics.StepCount;
		m_Statistics.SimulatedTime += deltaSeconds;
		UpdateStatistics();
	}

	void TerrainClimateRuntime::AdvectAtmosphericMoisture(
		const std::vector<float>& source, std::vector<float>& destination,
		float deltaSeconds) const
	{
		destination.assign(source.size(), 0.0f);
		const float xFraction = std::abs(m_Specification.WindVelocity.x)
			* deltaSeconds / m_Specification.CellSize;
		const float yFraction = std::abs(m_Specification.WindVelocity.y)
			* deltaSeconds / m_Specification.CellSize;
		for (uint32_t y = 0; y < m_Specification.Height; ++y)
		{
			for (uint32_t x = 0; x < m_Specification.Width; ++x)
			{
				const size_t index = Index(x, y);
				int targetX = static_cast<int>(x);
				int targetY = static_cast<int>(y);
				float activeXFraction = 0.0f;
				float activeYFraction = 0.0f;
				if (m_Specification.WindVelocity.x > 0.0f
					&& x + 1 < m_Specification.Width)
				{
					targetX = static_cast<int>(x + 1);
					activeXFraction = xFraction;
				}
				else if (m_Specification.WindVelocity.x < 0.0f && x > 0)
				{
					targetX = static_cast<int>(x - 1);
					activeXFraction = xFraction;
				}
				if (m_Specification.WindVelocity.y > 0.0f
					&& y + 1 < m_Specification.Height)
				{
					targetY = static_cast<int>(y + 1);
					activeYFraction = yFraction;
				}
				else if (m_Specification.WindVelocity.y < 0.0f && y > 0)
				{
					targetY = static_cast<int>(y - 1);
					activeYFraction = yFraction;
				}

				const float totalFraction = activeXFraction + activeYFraction;
				const float transportScale = totalFraction > 1.0f
					? 1.0f / totalFraction : 1.0f;
				activeXFraction *= transportScale;
				activeYFraction *= transportScale;
				const float retainedFraction = std::max(
					1.0f - activeXFraction - activeYFraction, 0.0f);
				destination[index] += source[index] * retainedFraction;
				if (activeXFraction > 0.0f)
					destination[Index(static_cast<uint32_t>(targetX), y)] +=
						source[index] * activeXFraction;
				if (activeYFraction > 0.0f)
					destination[Index(x, static_cast<uint32_t>(targetY))] +=
						source[index] * activeYFraction;
			}
		}
	}

	float TerrainClimateRuntime::CalculateSaturationMoisture(
		float temperature) const
	{
		const float exponent = glm::clamp(
			m_Specification.SaturationTemperatureSensitivity
				* (temperature - m_Specification.SaturationReferenceTemperature),
			-20.0f, 20.0f);
		return m_Specification.SaturationMoistureDepth * std::exp(exponent);
	}

	glm::vec2 TerrainClimateRuntime::CalculateTerrainGradient(
		uint32_t x, uint32_t y) const
	{
		const uint32_t left = x > 0 ? x - 1 : x;
		const uint32_t right = x + 1 < m_Specification.Width ? x + 1 : x;
		const uint32_t down = y > 0 ? y - 1 : y;
		const uint32_t up = y + 1 < m_Specification.Height ? y + 1 : y;
		const float xDistance = std::max(
			static_cast<float>(right - left) * m_Specification.CellSize,
			m_Specification.CellSize);
		const float yDistance = std::max(
			static_cast<float>(up - down) * m_Specification.CellSize,
			m_Specification.CellSize);
		return {
			(m_State.TerrainHeight[Index(right, y)]
				- m_State.TerrainHeight[Index(left, y)]) / xDistance,
			(m_State.TerrainHeight[Index(x, up)]
				- m_State.TerrainHeight[Index(x, down)]) / yDistance
		};
	}

	void TerrainClimateRuntime::SetTemperature(
		const std::vector<float>& temperature)
	{
		const bool valid = temperature.size() == m_State.Temperature.size()
			&& std::all_of(temperature.begin(), temperature.end(),
				[](float value) { return std::isfinite(value); });
		if (!valid)
			throw std::invalid_argument(std::string{});
		m_InitialState.Temperature = temperature;
		Reset();
	}

	void TerrainClimateRuntime::SetAtmosphericMoisture(
		const std::vector<float>& moistureDepth)
	{
		const bool valid = moistureDepth.size()
			== m_State.AtmosphericMoisture.size()
			&& std::all_of(moistureDepth.begin(), moistureDepth.end(),
				IsFiniteNonNegative);
		if (!valid)
			throw std::invalid_argument(std::string{});
		m_InitialState.AtmosphericMoisture = moistureDepth;
		Reset();
	}

	void TerrainClimateRuntime::SetSurfaceWater(
		const std::vector<float>& waterDepth)
	{
		const bool valid = waterDepth.size() == m_State.SurfaceWater.size()
			&& std::all_of(waterDepth.begin(), waterDepth.end(),
				IsFiniteNonNegative);
		if (!valid)
			throw std::invalid_argument(std::string{});
		m_InitialState.SurfaceWater = waterDepth;
		Reset();
	}

	void TerrainClimateRuntime::SetVegetationPotential(
		const std::vector<float>& potential)
	{
		const bool valid = potential.size()
			== m_State.VegetationPotential.size()
			&& std::all_of(potential.begin(), potential.end(), IsFiniteUnit);
		if (!valid)
			throw std::invalid_argument(std::string{});
		m_InitialState.VegetationPotential = potential;
		Reset();
	}

	void TerrainClimateRuntime::UpdateStatistics()
	{
		const double cellArea = static_cast<double>(m_Specification.CellSize)
			* m_Specification.CellSize;
		double atmosphericVolume = 0.0;
		double surfaceVolume = 0.0;
		float minimumTemperature = std::numeric_limits<float>::max();
		float maximumTemperature = -std::numeric_limits<float>::max();
		float minimumMoisture = std::numeric_limits<float>::max();
		float maximumMoisture = 0.0f;
		float maximumRainfall = 0.0f;
		float minimumVegetation = std::numeric_limits<float>::max();
		float maximumVegetation = 0.0f;
		bool finite = true;

		for (size_t index = 0; index < m_State.Temperature.size(); ++index)
		{
			const float temperature = m_State.Temperature[index];
			const float moisture = m_State.AtmosphericMoisture[index];
			const float surfaceWater = m_State.SurfaceWater[index];
			const float rainfall = m_State.Rainfall[index];
			const float vegetation = m_State.VegetationPotential[index];
			finite &= std::isfinite(temperature)
				&& IsFiniteNonNegative(moisture)
				&& IsFiniteNonNegative(surfaceWater)
				&& IsFiniteNonNegative(rainfall)
				&& IsFiniteUnit(vegetation);
			atmosphericVolume += static_cast<double>(moisture) * cellArea;
			surfaceVolume += static_cast<double>(surfaceWater) * cellArea;
			minimumTemperature = std::min(minimumTemperature, temperature);
			maximumTemperature = std::max(maximumTemperature, temperature);
			minimumMoisture = std::min(minimumMoisture, moisture);
			maximumMoisture = std::max(maximumMoisture, moisture);
			maximumRainfall = std::max(maximumRainfall, rainfall);
			minimumVegetation = std::min(minimumVegetation, vegetation);
			maximumVegetation = std::max(maximumVegetation, vegetation);
		}

		m_Statistics.AtmosphericWaterVolume = atmosphericVolume;
		m_Statistics.SurfaceWaterVolume = surfaceVolume;
		m_Statistics.WaterBudgetError = atmosphericVolume + surfaceVolume
			- m_Statistics.InitialWaterVolume;
		const bool empty = m_State.Temperature.empty();
		m_Statistics.MinimumTemperature = empty ? 0.0f : minimumTemperature;
		m_Statistics.MaximumTemperature = empty ? 0.0f : maximumTemperature;
		m_Statistics.MinimumAtmosphericMoisture = empty ? 0.0f : minimumMoisture;
		m_Statistics.MaximumAtmosphericMoisture = maximumMoisture;
		m_Statistics.MaximumRainfall = maximumRainfall;
		m_Statistics.MinimumVegetationPotential = empty ? 0.0f : minimumVegetation;
		m_Statistics.MaximumVegetationPotential = maximumVegetation;
		m_Statistics.Finite = finite;
		m_Statistics.Accumulator = m_Accumulator;
	}

	size_t TerrainClimateRuntime::Index(uint32_t x, uint32_t y) const
	{
		return static_cast<size_t>(y) * m_Specification.Width + x;
	}
}
