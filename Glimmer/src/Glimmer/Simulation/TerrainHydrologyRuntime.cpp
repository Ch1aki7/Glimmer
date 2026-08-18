#include "glpch.h"
#include "Glimmer/Simulation/TerrainHydrologyRuntime.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace gl {
	namespace {
		constexpr float SedimentCapacityEpsilon = 1.0e-6f;
		constexpr float MaximumSedimentSaturation = 1000.0f;
	}

	TerrainHydrologyRuntime::TerrainHydrologyRuntime(
		const TerrainHydrologySpecification& specification,
		const std::vector<float>& heightField)
		: m_Specification(specification), m_InitialHeight(heightField)
	{
		if (specification.Width == 0 || specification.Height == 0
			|| specification.CellSize <= 0.0f
			|| specification.FixedTimeStep <= 0.0f
			|| specification.MaxSubsteps == 0
			|| specification.Gravity < 0.0f
			|| specification.SedimentCapacityScale < 0.0f
			|| specification.ErosionRate < 0.0f
			|| specification.DepositionRate < 0.0f
			|| specification.TerrainDensity <= 0.0f
			|| specification.MaximumErosionDepth < 0.0f
			|| specification.MaximumHeightChangePerStep < 0.0f
			|| !std::isfinite(specification.CellSize)
			|| !std::isfinite(specification.FixedTimeStep)
			|| !std::isfinite(specification.Gravity)
			|| !std::isfinite(specification.SedimentCapacityScale)
			|| !std::isfinite(specification.ErosionRate)
			|| !std::isfinite(specification.DepositionRate)
			|| !std::isfinite(specification.TerrainDensity)
			|| !std::isfinite(specification.MaximumErosionDepth)
			|| !std::isfinite(specification.MaximumHeightChangePerStep))
		{
			throw std::invalid_argument(
				"Terrain hydrology specification is invalid.");
		}

		const size_t cellCount = static_cast<size_t>(specification.Width)
			* specification.Height;
		if (heightField.size() != cellCount
			|| !std::all_of(heightField.begin(), heightField.end(),
				[](float value) { return std::isfinite(value); }))
		{
			throw std::invalid_argument(
				"Terrain hydrology height field is invalid.");
		}

		m_InitialWater.assign(cellCount, 0.0f);
		m_InitialSediment.assign(cellCount, 0.0f);
		Reset();
	}

	uint32_t TerrainHydrologyRuntime::Advance(float frameDeltaSeconds)
	{
		if (!m_Playing || !std::isfinite(frameDeltaSeconds)
			|| frameDeltaSeconds <= 0.0f)
			return 0;

		m_Accumulator += static_cast<double>(frameDeltaSeconds);
		const double fixedTimeStep = m_Specification.FixedTimeStep;
		uint32_t substeps = 0;
		while (m_Accumulator + std::numeric_limits<double>::epsilon()
			>= fixedTimeStep
			&& substeps < m_Specification.MaxSubsteps)
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

	bool TerrainHydrologyRuntime::SingleStep()
	{
		if (m_Playing)
			return false;
		Step(m_Specification.FixedTimeStep);
		return true;
	}

	void TerrainHydrologyRuntime::Reset()
	{
		m_Playing = false;
		m_Accumulator = 0.0;
		m_State.Height = m_InitialHeight;
		m_State.Water = m_InitialWater;
		m_State.Flux.assign(m_State.Height.size(), glm::vec4(0.0f));
		m_State.Velocity.assign(m_State.Height.size(), glm::vec2(0.0f));
		m_State.Sediment = m_InitialSediment;
		m_State.SedimentCapacity.assign(m_State.Height.size(), 0.0f);
		m_State.SedimentSaturation.assign(m_State.Height.size(), 0.0f);
		m_Statistics = {};
		UpdateSedimentDiagnostics();
		UpdateStatistics();
		m_Statistics.InitialWaterVolume = m_Statistics.WaterVolume;
		m_Statistics.InitialSedimentMass = m_Statistics.SedimentMass;
		m_Statistics.InitialTerrainMass = m_Statistics.TerrainMass;
		m_Statistics.MassError = 0.0;
		m_Statistics.SedimentMassError = 0.0;
		m_Statistics.TerrainSedimentMassError = 0.0;
	}

	void TerrainHydrologyRuntime::SetWaterDepth(
		const std::vector<float>& waterDepth)
	{
		if (waterDepth.size() != m_State.Water.size()
			|| !std::all_of(waterDepth.begin(), waterDepth.end(),
				[](float value) {
					return std::isfinite(value) && value >= 0.0f;
				}))
		{
			throw std::invalid_argument(
				"Terrain hydrology water field is invalid.");
		}

		m_InitialWater = waterDepth;
		Reset();
	}

	void TerrainHydrologyRuntime::SetSedimentDensity(
		const std::vector<float>& sedimentDensity)
	{
		if (sedimentDensity.size() != m_State.Sediment.size()
			|| !std::all_of(sedimentDensity.begin(), sedimentDensity.end(),
				[](float value) {
					return std::isfinite(value) && value >= 0.0f;
				}))
		{
			throw std::invalid_argument(
				"Terrain hydrology sediment field is invalid.");
		}

		m_InitialSediment = sedimentDensity;
		Reset();
	}

	void TerrainHydrologyRuntime::SetSedimentCapacityScale(float capacityScale)
	{
		m_Specification.SedimentCapacityScale = std::isfinite(capacityScale)
			? std::max(capacityScale, 0.0f) : 0.0f;
		UpdateSedimentDiagnostics();
		UpdateStatistics();
	}

	void TerrainHydrologyRuntime::SetRainfallRate(float rainfallRate)
	{
		m_Specification.RainfallRate =
			std::isfinite(rainfallRate) ? std::max(rainfallRate, 0.0f) : 0.0f;
	}

	void TerrainHydrologyRuntime::Step(float deltaSeconds)
	{
		const uint32_t width = m_Specification.Width;
		const uint32_t height = m_Specification.Height;
		const float cellSize = m_Specification.CellSize;
		const float cellArea = cellSize * cellSize;
		const float gravityScale = m_Specification.Gravity
			* deltaSeconds / cellSize;
		const float damping = glm::clamp(
			m_Specification.FluxDamping, 0.0f, 1.0f);
		const float rainfallDepth = std::max(
			m_Specification.RainfallRate, 0.0f) * deltaSeconds;

		std::vector<glm::vec4> nextFlux(m_State.Flux.size(), glm::vec4(0.0f));
		for (uint32_t y = 0; y < height; ++y)
		{
			for (uint32_t x = 0; x < width; ++x)
			{
				const size_t index = Index(x, y);
				const float surface = m_State.Height[index]
					+ m_State.Water[index] + rainfallDepth;
				glm::vec4 flux(0.0f);
				auto solveDirection = [&](int component, uint32_t nx, uint32_t ny) {
					const size_t neighbour = Index(nx, ny);
					const float neighbourSurface = m_State.Height[neighbour]
						+ m_State.Water[neighbour] + rainfallDepth;
					flux[component] = std::max(0.0f,
						m_State.Flux[index][component] * damping
						+ gravityScale * (surface - neighbourSurface));
				};
				if (x > 0) solveDirection(0, x - 1, y);
				if (x + 1 < width) solveDirection(1, x + 1, y);
				if (y > 0) solveDirection(2, x, y - 1);
				if (y + 1 < height) solveDirection(3, x, y + 1);

				const float outgoing = flux.x + flux.y + flux.z + flux.w;
				if (outgoing > 0.0f)
				{
					const float availableVolume =
						(m_State.Water[index] + rainfallDepth) * cellArea;
					const float scale = std::min(
						1.0f, availableVolume / (outgoing * deltaSeconds));
					flux *= scale;
				}
				nextFlux[index] = flux;
			}
		}

		std::vector<glm::vec4> sedimentFlux(
			m_State.Sediment.size(), glm::vec4(0.0f));
		for (size_t index = 0; index < m_State.Sediment.size(); ++index)
		{
			const float sedimentMass = m_State.Sediment[index] * cellArea;
			const float availableWaterVolume =
				(m_State.Water[index] + rainfallDepth) * cellArea;
			if (sedimentMass <= 0.0f || availableWaterVolume <= 1.0e-12f)
				continue;

			const float concentration = sedimentMass / availableWaterVolume;
			sedimentFlux[index] = nextFlux[index] * concentration;
			const float outgoingMassRate = glm::dot(
				sedimentFlux[index], glm::vec4(1.0f));
			if (outgoingMassRate > 0.0f)
			{
				const float scale = std::min(
					1.0f, sedimentMass / (outgoingMassRate * deltaSeconds));
				sedimentFlux[index] *= scale;
			}
		}

		std::vector<float> nextWater(m_State.Water.size(), 0.0f);
		std::vector<float> nextSediment(m_State.Sediment.size(), 0.0f);
		for (uint32_t y = 0; y < height; ++y)
		{
			for (uint32_t x = 0; x < width; ++x)
			{
				const size_t index = Index(x, y);
				const glm::vec4 outgoingFlux = nextFlux[index];
				const float outgoing = outgoingFlux.x + outgoingFlux.y
					+ outgoingFlux.z + outgoingFlux.w;
				float incoming = 0.0f;
				if (x > 0) incoming += nextFlux[Index(x - 1, y)].y;
				if (x + 1 < width) incoming += nextFlux[Index(x + 1, y)].x;
				if (y > 0) incoming += nextFlux[Index(x, y - 1)].w;
				if (y + 1 < height) incoming += nextFlux[Index(x, y + 1)].z;

				const float water = m_State.Water[index] + rainfallDepth
					+ (incoming - outgoing) * deltaSeconds / cellArea;
				nextWater[index] = std::max(water, 0.0f);
				const float averageDepth = std::max(
					0.5f * (m_State.Water[index] + nextWater[index]), 1.0e-6f);
				const float incomingFromLeft = x > 0
					? nextFlux[Index(x - 1, y)].y : 0.0f;
				const float incomingFromRight = x + 1 < width
					? nextFlux[Index(x + 1, y)].x : 0.0f;
				const float incomingFromDown = y > 0
					? nextFlux[Index(x, y - 1)].w : 0.0f;
				const float incomingFromUp = y + 1 < height
					? nextFlux[Index(x, y + 1)].z : 0.0f;
				m_State.Velocity[index] = {
					0.5f * (outgoingFlux.y - incomingFromRight
						+ incomingFromLeft - outgoingFlux.x)
						/ (averageDepth * cellSize),
					0.5f * (outgoingFlux.w - incomingFromUp
						+ incomingFromDown - outgoingFlux.z)
						/ (averageDepth * cellSize)
				};

				const glm::vec4 outgoingSediment = sedimentFlux[index];
				float incomingSediment = 0.0f;
				if (x > 0)
					incomingSediment += sedimentFlux[Index(x - 1, y)].y;
				if (x + 1 < width)
					incomingSediment += sedimentFlux[Index(x + 1, y)].x;
				if (y > 0)
					incomingSediment += sedimentFlux[Index(x, y - 1)].w;
				if (y + 1 < height)
					incomingSediment += sedimentFlux[Index(x, y + 1)].z;
				const float outgoingSedimentRate = glm::dot(
					outgoingSediment, glm::vec4(1.0f));
				const float sedimentMass = m_State.Sediment[index] * cellArea
					+ (incomingSediment - outgoingSedimentRate) * deltaSeconds;
				nextSediment[index] = std::max(sedimentMass / cellArea, 0.0f);
			}
		}

		m_State.Flux = std::move(nextFlux);
		m_State.Water = std::move(nextWater);
		m_State.Sediment = std::move(nextSediment);
		UpdateSedimentDiagnostics();
		ApplyErosionDeposition(deltaSeconds);
		UpdateSedimentDiagnostics();
		++m_Statistics.StepCount;
		m_Statistics.SimulatedTime += deltaSeconds;
		m_Statistics.RainfallVolume += static_cast<double>(rainfallDepth)
			* cellArea * m_State.Water.size();
		UpdateStatistics();
	}

	void TerrainHydrologyRuntime::ApplyErosionDeposition(float deltaSeconds)
	{
		const float erosionRate = std::max(m_Specification.ErosionRate, 0.0f);
		const float depositionRate =
			std::max(m_Specification.DepositionRate, 0.0f);
		const float terrainDensity =
			std::max(m_Specification.TerrainDensity, 1.0e-6f);
		const float maximumHeightChange = std::max(
			m_Specification.MaximumHeightChangePerStep, 0.0f);
		if ((erosionRate <= 0.0f && depositionRate <= 0.0f)
			|| maximumHeightChange <= 0.0f)
		{
			m_Statistics.MaximumAbsoluteHeightChangePerStep = 0.0f;
			return;
		}

		const double cellArea = static_cast<double>(m_Specification.CellSize)
			* m_Specification.CellSize;
		float largestHeightChange = 0.0f;
		for (size_t index = 0; index < m_State.Height.size(); ++index)
		{
			const float sediment = m_State.Sediment[index];
			const float capacity = m_State.SedimentCapacity[index];
			const float erosionFloor = m_InitialHeight[index]
				- m_Specification.MaximumErosionDepth;
			float transferDensity = 0.0f;

			if (capacity > sediment && erosionRate > 0.0f)
			{
				const float availableTerrainDensity = std::max(
					(m_State.Height[index] - erosionFloor) * terrainDensity,
					0.0f);
				transferDensity = std::min({
					(capacity - sediment) * erosionRate * deltaSeconds,
					maximumHeightChange * terrainDensity,
					availableTerrainDensity
				});
				m_State.Height[index] -= transferDensity / terrainDensity;
				m_State.Sediment[index] += transferDensity;
				m_Statistics.CumulativeErodedMass +=
					static_cast<double>(transferDensity) * cellArea;
			}
			else if (sediment > capacity && depositionRate > 0.0f)
			{
				transferDensity = std::min({
					(sediment - capacity) * depositionRate * deltaSeconds,
					maximumHeightChange * terrainDensity,
					sediment
				});
				m_State.Height[index] += transferDensity / terrainDensity;
				m_State.Sediment[index] -= transferDensity;
				m_Statistics.CumulativeDepositedMass +=
					static_cast<double>(transferDensity) * cellArea;
			}

			largestHeightChange = std::max(
				largestHeightChange, transferDensity / terrainDensity);
		}
		m_Statistics.MaximumAbsoluteHeightChangePerStep = largestHeightChange;
	}

	void TerrainHydrologyRuntime::UpdateSedimentDiagnostics()
	{
		const float capacityScale = std::max(
			m_Specification.SedimentCapacityScale, 0.0f);
		for (size_t index = 0; index < m_State.Sediment.size(); ++index)
		{
			const float speed = glm::length(m_State.Velocity[index]);
			const float capacity = capacityScale
				* m_State.Water[index] * speed;
			m_State.SedimentCapacity[index] = capacity;
			m_State.SedimentSaturation[index] = capacity > SedimentCapacityEpsilon
				? std::min(m_State.Sediment[index] / capacity,
					MaximumSedimentSaturation)
				: (m_State.Sediment[index] > SedimentCapacityEpsilon
					? MaximumSedimentSaturation : 0.0f);
		}
	}

	void TerrainHydrologyRuntime::UpdateStatistics()
	{
		const double cellArea = static_cast<double>(m_Specification.CellSize)
			* m_Specification.CellSize;
		double waterVolume = 0.0;
		double sedimentMass = 0.0;
		double terrainMass = 0.0;
		float minimumWater = std::numeric_limits<float>::max();
		float maximumWater = 0.0f;
		float maximumSpeed = 0.0f;
		float minimumSediment = std::numeric_limits<float>::max();
		float maximumSediment = 0.0f;
		float minimumCapacity = std::numeric_limits<float>::max();
		float maximumCapacity = 0.0f;
		float minimumSaturation = std::numeric_limits<float>::max();
		float maximumSaturation = 0.0f;
		float minimumTerrainHeight = std::numeric_limits<float>::max();
		float maximumTerrainHeight = -std::numeric_limits<float>::max();
		bool finite = true;
		for (size_t index = 0; index < m_State.Water.size(); ++index)
		{
			const float water = m_State.Water[index];
			const float speed = glm::length(m_State.Velocity[index]);
			const float sediment = m_State.Sediment[index];
			const float capacity = m_State.SedimentCapacity[index];
			const float saturation = m_State.SedimentSaturation[index];
			const float terrainHeight = m_State.Height[index];
			finite &= std::isfinite(water) && water >= 0.0f
				&& std::isfinite(speed)
				&& std::isfinite(sediment) && sediment >= 0.0f
				&& std::isfinite(capacity) && capacity >= 0.0f
				&& std::isfinite(saturation) && saturation >= 0.0f
				&& std::isfinite(terrainHeight)
				&& terrainHeight >= m_InitialHeight[index]
					- m_Specification.MaximumErosionDepth - 1.0e-6f;
			waterVolume += static_cast<double>(water) * cellArea;
			sedimentMass += static_cast<double>(sediment) * cellArea;
			terrainMass += static_cast<double>(terrainHeight)
				* m_Specification.TerrainDensity * cellArea;
			minimumWater = std::min(minimumWater, water);
			maximumWater = std::max(maximumWater, water);
			maximumSpeed = std::max(maximumSpeed, speed);
			minimumSediment = std::min(minimumSediment, sediment);
			maximumSediment = std::max(maximumSediment, sediment);
			minimumCapacity = std::min(minimumCapacity, capacity);
			maximumCapacity = std::max(maximumCapacity, capacity);
			minimumSaturation = std::min(minimumSaturation, saturation);
			maximumSaturation = std::max(maximumSaturation, saturation);
			minimumTerrainHeight = std::min(minimumTerrainHeight, terrainHeight);
			maximumTerrainHeight = std::max(maximumTerrainHeight, terrainHeight);
		}
		m_Statistics.WaterVolume = waterVolume;
		m_Statistics.MassError = waterVolume
			- m_Statistics.InitialWaterVolume - m_Statistics.RainfallVolume;
		m_Statistics.MinimumWaterDepth = m_State.Water.empty()
			? 0.0f : minimumWater;
		m_Statistics.MaximumWaterDepth = maximumWater;
		m_Statistics.MaximumSpeed = maximumSpeed;
		m_Statistics.SedimentMass = sedimentMass;
		m_Statistics.SedimentMassError = sedimentMass
			+ m_Statistics.SedimentBoundaryLoss
			- m_Statistics.InitialSedimentMass;
		m_Statistics.TerrainMass = terrainMass;
		m_Statistics.TerrainSedimentMassError = terrainMass + sedimentMass
			+ m_Statistics.SedimentBoundaryLoss
			- m_Statistics.InitialTerrainMass
			- m_Statistics.InitialSedimentMass;
		m_Statistics.MinimumSediment = m_State.Sediment.empty()
			? 0.0f : minimumSediment;
		m_Statistics.MaximumSediment = maximumSediment;
		m_Statistics.MinimumSedimentCapacity = m_State.Sediment.empty()
			? 0.0f : minimumCapacity;
		m_Statistics.MaximumSedimentCapacity = maximumCapacity;
		m_Statistics.MinimumSedimentSaturation = m_State.Sediment.empty()
			? 0.0f : minimumSaturation;
		m_Statistics.MaximumSedimentSaturation = maximumSaturation;
		m_Statistics.MinimumTerrainHeight = m_State.Height.empty()
			? 0.0f : minimumTerrainHeight;
		m_Statistics.MaximumTerrainHeight = m_State.Height.empty()
			? 0.0f : maximumTerrainHeight;
		m_Statistics.Finite = finite;
		m_Statistics.Accumulator = m_Accumulator;
	}

	size_t TerrainHydrologyRuntime::Index(uint32_t x, uint32_t y) const
	{
		return static_cast<size_t>(y) * m_Specification.Width + x;
	}

}
