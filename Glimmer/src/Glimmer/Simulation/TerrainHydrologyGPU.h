#pragma once

#include "Glimmer/Renderer/ComputeShader.h"
#include "Glimmer/Simulation/SimulationGrid.h"

#include <filesystem>
#include <string>

namespace gl {

	struct TerrainHydrologyGPUSettings
	{
		float FixedTimeStep = 1.0f / 60.0f;
		uint32_t MaxSubsteps = 4;
		float Gravity = 9.81f;
		float FluxDamping = 0.995f;
		float RainfallRate = 0.02f;
	};

	struct TerrainHydrologyGPUStatistics
	{
		uint64_t StepCount = 0;
		double SimulatedTime = 0.0;
		double Accumulator = 0.0;
		double DroppedTime = 0.0;
		double ExpectedWaterVolume = 0.0;
		double WaterVolume = 0.0;
		double MassError = 0.0;
		float MinimumWaterDepth = 0.0f;
		float MaximumWaterDepth = 0.0f;
		float MaximumSpeed = 0.0f;
		bool Finite = true;
		bool ReadbackAvailable = false;
	};

	struct TerrainHydrologyGPUValidationResult
	{
		bool Attempted = false;
		bool Passed = false;
		bool Finite = false;
		bool MassConserved = false;
		bool BasinAccumulation = false;
		bool FramePartitionIndependent = false;
		double RelativeMassError = 0.0;
		float BasinDepth = 0.0f;
		float MaximumRimDepth = 0.0f;
		float MaximumPartitionDifference = 0.0f;
		std::string Message;
	};

	class TerrainHydrologyGPU
	{
	public:
		TerrainHydrologyGPU(uint32_t width, uint32_t height,
			std::filesystem::path fluxShaderPath,
			std::filesystem::path updateShaderPath);

		uint32_t Advance(float frameDeltaSeconds,
			const Ref<Texture2D>& heightMap, float heightScale, float worldSize);
		void SingleStep(const Ref<Texture2D>& heightMap,
			float heightScale, float worldSize);
		void Reset();
		void ReadbackStatistics(float worldSize);
		bool ReloadShadersIfChanged();
		static TerrainHydrologyGPUValidationResult ValidateContract(
			const std::filesystem::path& fluxShaderPath,
			const std::filesystem::path& updateShaderPath);

		const Ref<Texture2D>& GetWaterTexture() const
		{
			return m_Water.ReadTexture();
		}
		const Ref<Texture2D>& GetVelocityTexture() const
		{
			return m_Velocity.ReadTexture();
		}
		TerrainHydrologyGPUSettings& GetSettings() { return m_Settings; }
		const TerrainHydrologyGPUStatistics& GetStatistics() const
		{
			return m_Statistics;
		}

	private:
		void Step(const Ref<Texture2D>& heightMap,
			float heightScale, float worldSize);
		void Dispatch(const Ref<ComputeShader>& shader) const;

		SimulationGrid m_Water;
		SimulationGrid m_Flux;
		SimulationGrid m_Velocity;
		Ref<ComputeShader> m_FluxShader;
		Ref<ComputeShader> m_UpdateShader;
		TerrainHydrologyGPUSettings m_Settings;
		TerrainHydrologyGPUStatistics m_Statistics;
		double m_Accumulator = 0.0;
	};

}
