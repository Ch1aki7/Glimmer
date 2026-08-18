#pragma once

#include "Glimmer/Core/Core.h"

#include <glm/glm.hpp>
#include <vector>

namespace gl {

	struct TerrainHydrologySpecification
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		float CellSize = 1.0f;
		float FixedTimeStep = 1.0f / 60.0f;
		uint32_t MaxSubsteps = 8;
		float Gravity = 9.81f;
		float FluxDamping = 0.995f;
		float RainfallRate = 0.0f;
		float SedimentCapacityScale = 1.0f;
	};

	struct TerrainHydrologyStatistics
	{
		uint64_t StepCount = 0;
		double SimulatedTime = 0.0;
		double Accumulator = 0.0;
		double DroppedTime = 0.0;
		double InitialWaterVolume = 0.0;
		double RainfallVolume = 0.0;
		double WaterVolume = 0.0;
		double MassError = 0.0;
		float MinimumWaterDepth = 0.0f;
		float MaximumWaterDepth = 0.0f;
		float MaximumSpeed = 0.0f;
		double InitialSedimentMass = 0.0;
		double SedimentMass = 0.0;
		double SedimentBoundaryLoss = 0.0;
		double SedimentMassError = 0.0;
		float MinimumSediment = 0.0f;
		float MaximumSediment = 0.0f;
		float MinimumSedimentCapacity = 0.0f;
		float MaximumSedimentCapacity = 0.0f;
		float MinimumSedimentSaturation = 0.0f;
		float MaximumSedimentSaturation = 0.0f;
		bool Finite = true;
	};

	struct TerrainHydrologyState
	{
		std::vector<float> Height;
		std::vector<float> Water;
		std::vector<glm::vec4> Flux;
		std::vector<glm::vec2> Velocity;
		// Suspended sediment mass per unit terrain area.
		std::vector<float> Sediment;
		// Derived diagnostics; neither field modifies Height or Sediment.
		std::vector<float> SedimentCapacity;
		std::vector<float> SedimentSaturation;
	};

	class TerrainHydrologyRuntime
	{
	public:
		explicit TerrainHydrologyRuntime(
			const TerrainHydrologySpecification& specification,
			const std::vector<float>& heightField);

		void Play() { m_Playing = true; }
		void Pause() { m_Playing = false; }
		bool IsPlaying() const { return m_Playing; }
		uint32_t Advance(float frameDeltaSeconds);
		bool SingleStep();
		void Reset();
		void SetWaterDepth(const std::vector<float>& waterDepth);
		void SetSedimentDensity(const std::vector<float>& sedimentDensity);
		void SetSedimentCapacityScale(float capacityScale);
		void SetRainfallRate(float rainfallRate);

		const TerrainHydrologySpecification& GetSpecification() const
		{
			return m_Specification;
		}
		const TerrainHydrologyState& GetState() const { return m_State; }
		const TerrainHydrologyStatistics& GetStatistics() const
		{
			return m_Statistics;
		}

	private:
		void Step(float deltaSeconds);
		void UpdateSedimentDiagnostics();
		void UpdateStatistics();
		size_t Index(uint32_t x, uint32_t y) const;

		TerrainHydrologySpecification m_Specification;
		TerrainHydrologyState m_State;
		std::vector<float> m_InitialHeight;
		std::vector<float> m_InitialWater;
		std::vector<float> m_InitialSediment;
		TerrainHydrologyStatistics m_Statistics;
		bool m_Playing = false;
		double m_Accumulator = 0.0;
	};

}
