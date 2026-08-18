#pragma once

#include "Glimmer/Renderer/ComputeShader.h"
#include "Glimmer/Simulation/SimulationGrid.h"

#include <filesystem>
#include <string>
#include <vector>

namespace gl {

	struct TerrainHydrologyGPUSettings
	{
		float FixedTimeStep = 1.0f / 60.0f;
		uint32_t MaxSubsteps = 4;
		float Gravity = 9.81f;
		float FluxDamping = 0.995f;
		float RainfallRate = 0.02f;
		float SedimentCapacityScale = 1.0f;
		float ErosionRate = 0.0f;
		float DepositionRate = 0.0f;
		float TerrainDensity = 1.0f;
		float MaximumErosionDepth = 1.0f;
		float MaximumHeightChangePerStep = 0.01f;
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
		double InitialSedimentMass = 0.0;
		double SedimentMass = 0.0;
		double SedimentMassError = 0.0;
		double InitialTerrainMass = 0.0;
		double TerrainMass = 0.0;
		double ErodedMass = 0.0;
		double DepositedMass = 0.0;
		double TerrainSedimentMassError = 0.0;
		float MinimumWaterDepth = 0.0f;
		float MaximumWaterDepth = 0.0f;
		float MaximumSpeed = 0.0f;
		float MinimumSediment = 0.0f;
		float MaximumSediment = 0.0f;
		float MinimumSedimentCapacity = 0.0f;
		float MaximumSedimentCapacity = 0.0f;
		float MinimumSedimentSaturation = 0.0f;
		float MaximumSedimentSaturation = 0.0f;
		float MinimumTerrainHeight = 0.0f;
		float MaximumTerrainHeight = 0.0f;
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
		bool SedimentMassConserved = false;
		bool SedimentMovedDownstream = false;
		bool SedimentFramePartitionIndependent = false;
		bool SedimentCapacityValid = false;
		bool SedimentCapacityFramePartitionIndependent = false;
		bool ErosionDepositionValid = false;
		bool ErosionDepositionFramePartitionIndependent = false;
		bool ErosionResetValid = false;
		double RelativeMassError = 0.0;
		double RelativeSedimentMassError = 0.0;
		float BasinDepth = 0.0f;
		float MaximumRimDepth = 0.0f;
		float MaximumPartitionDifference = 0.0f;
		float DownstreamSediment = 0.0f;
		float SourceSediment = 0.0f;
		float MaximumSedimentPartitionDifference = 0.0f;
		float MaximumSedimentCapacity = 0.0f;
		float MaximumSedimentSaturation = 0.0f;
		float MaximumCapacityPartitionDifference = 0.0f;
		float MaximumSaturationPartitionDifference = 0.0f;
		double RelativeTerrainSedimentMassError = 0.0;
		float ErodedHeight = 0.0f;
		float DepositedHeight = 0.0f;
		float MaximumErosionHeightPartitionDifference = 0.0f;
		float MaximumErosionSedimentPartitionDifference = 0.0f;
		std::string Message;
	};

	class TerrainHydrologyGPU
	{
	public:
		TerrainHydrologyGPU(uint32_t width, uint32_t height,
			std::filesystem::path fluxShaderPath,
			std::filesystem::path updateShaderPath,
			std::filesystem::path sedimentShaderPath,
			std::filesystem::path capacityShaderPath,
			std::filesystem::path erosionShaderPath);

		uint32_t Advance(float frameDeltaSeconds,
			const Ref<Texture2D>& heightMap, float heightScale, float worldSize);
		void SingleStep(const Ref<Texture2D>& heightMap,
			float heightScale, float worldSize);
		void Reset();
		void SetInitialHeightMap(const Ref<Texture2D>& heightMap,
			float heightScale, float worldSize);
		void SetSedimentDensity(const std::vector<float>& sedimentDensity,
			float worldSize);
		void SetUniformSedimentDensity(float sedimentDensity, float worldSize);
		void SetSedimentCapacityScale(float capacityScale);
		void ReadbackStatistics(float worldSize, float heightScale);
		bool ReloadShadersIfChanged();
		static TerrainHydrologyGPUValidationResult ValidateContract(
			const std::filesystem::path& fluxShaderPath,
			const std::filesystem::path& updateShaderPath,
			const std::filesystem::path& sedimentShaderPath,
			const std::filesystem::path& capacityShaderPath,
			const std::filesystem::path& erosionShaderPath);

		const Ref<Texture2D>& GetHeightTexture() const
		{
			return m_Height.ReadTexture();
		}

		const Ref<Texture2D>& GetWaterTexture() const
		{
			return m_Water.ReadTexture();
		}
		const Ref<Texture2D>& GetVelocityTexture() const
		{
			return m_Velocity.ReadTexture();
		}
		const Ref<Texture2D>& GetSedimentTexture() const
		{
			return m_Sediment.ReadTexture();
		}
		const Ref<Texture2D>& GetSedimentCapacityTexture() const
		{
			return m_SedimentCapacity;
		}
		const Ref<Texture2D>& GetSedimentSaturationTexture() const
		{
			return m_SedimentSaturation;
		}
		TerrainHydrologyGPUSettings& GetSettings() { return m_Settings; }
		const TerrainHydrologyGPUStatistics& GetStatistics() const
		{
			return m_Statistics;
		}

	private:
		void Step(const Ref<Texture2D>& heightMap,
			float heightScale, float worldSize);
		void UpdateSedimentDiagnostics();
		void ApplyErosionDeposition(float deltaSeconds, float heightScale);
		void Dispatch(const Ref<ComputeShader>& shader) const;

		SimulationGrid m_Height;
		SimulationGrid m_Water;
		SimulationGrid m_Flux;
		SimulationGrid m_Velocity;
		SimulationGrid m_Sediment;
		Ref<Texture2D> m_SedimentCapacity;
		Ref<Texture2D> m_SedimentSaturation;
		Ref<ComputeShader> m_FluxShader;
		Ref<ComputeShader> m_UpdateShader;
		Ref<ComputeShader> m_SedimentShader;
		Ref<ComputeShader> m_CapacityShader;
		Ref<ComputeShader> m_ErosionShader;
		Ref<Texture2D> m_InitialHeightTexture;
		std::vector<float> m_InitialHeightData;
		TerrainHydrologyGPUSettings m_Settings;
		TerrainHydrologyGPUStatistics m_Statistics;
		double m_Accumulator = 0.0;
	};

}
