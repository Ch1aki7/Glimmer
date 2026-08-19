#pragma once

#include "Glimmer/Scene/Components.h"
#include "Glimmer/Simulation/TerrainHydrologyGPU.h"
#include "Glimmer/Simulation/TerrainClimateGPU.h"
#include "Glimmer/Simulation/TerrainEnvironmentGPU.h"

namespace gl {
	class TerrainRenderer
	{
	public:
		enum class SamplingMode : int
		{
			FullFourLayers = 0,
			TopTwoLayers,
			TopTwoDominantDetail,
			AutomaticDistance
		};

		enum class HydrologyVisualizationMode : int
		{
			None = 0,
			Water,
			Sediment,
			SedimentCapacity,
			SedimentSaturation
		};

		enum class ClimateVisualizationMode : int
		{
			None = 0,
			Temperature,
			AtmosphericMoisture,
			Rainfall,
			VegetationPotential
		};

		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t CandidateChunks = 0;
			uint32_t SubmittedChunks = 0;
			uint32_t CulledChunks = 0;
			uint32_t SharedMeshes = 0;
			uint32_t LOD0Chunks = 0;
			uint32_t LOD1Chunks = 0;
			uint32_t LOD2Chunks = 0;
			uint64_t SubmittedTriangles = 0;
			uint32_t BoundMaterialTextures = 0;
			uint32_t RuntimeDerivedMapRefreshes = 0;
			float GpuMilliseconds = 0.0f;
			uint64_t GpuTimingSample = 0;
			bool GpuTimingAvailable = false;
			SamplingMode Mode = SamplingMode::FullFourLayers;
			float DetailDistance = 80.0f;
		};

		static void Init();
		static void Shutdown();
		static void BeginScene(float deltaSeconds = 0.0f);
		static void EndScene();
		static bool Prepare(TerrainComponent& component);
		static void Draw(TerrainComponent& component, const glm::mat4& transform,
			const glm::mat4& viewProjection, const glm::vec3& cameraPosition, int entityID);
		static void Invalidate(TerrainComponent& component);
		static void SetSamplingMode(SamplingMode mode);
		static SamplingMode GetSamplingMode();
		static void SetDetailDistance(float distance);
		static float GetDetailDistance();
		static void SetLODDistances(float middleDistance, float farDistance);
		static glm::vec2 GetLODDistances();
		static void SetLODVisualizationEnabled(bool enabled);
		static bool IsLODVisualizationEnabled();
		static void SetHydrologyPlaying(bool playing);
		static bool IsHydrologyPlaying();
		static void RequestHydrologySingleStep();
		static void RequestHydrologyReset();
		static void SetHydrologyRainfall(float rainfallRate);
		static float GetHydrologyRainfall();
		static void SetHydrologySedimentCapacityScale(float capacityScale);
		static float GetHydrologySedimentCapacityScale();
		static void SetHydrologyErosionRate(float erosionRate);
		static float GetHydrologyErosionRate();
		static void SetHydrologyDepositionRate(float depositionRate);
		static float GetHydrologyDepositionRate();
		static void SetHydrologyTerrainDensity(float terrainDensity);
		static float GetHydrologyTerrainDensity();
		static void SetHydrologyMaximumErosionDepth(float depth);
		static float GetHydrologyMaximumErosionDepth();
		static void SetHydrologyMaximumHeightChange(float heightChange);
		static float GetHydrologyMaximumHeightChange();
		static void SetHydrologyVisualizationMode(HydrologyVisualizationMode mode);
		static HydrologyVisualizationMode GetHydrologyVisualizationMode();
		static void SetHydrologySedimentSeedDensity(float sedimentDensity);
		static float GetHydrologySedimentSeedDensity();
		static void RequestHydrologySedimentSeed();
		static void RequestHydrologyReadback();
		static TerrainHydrologyGPUStatistics GetHydrologyStatistics();
		static void RequestHydrologyContractValidation();
		static TerrainHydrologyGPUValidationResult GetHydrologyValidationResult();
		static void SetClimatePlaying(bool playing);
		static bool IsClimatePlaying();
		static void RequestClimateSingleStep();
		static void RequestClimateReset();
		static void SetClimateWindVelocity(const glm::vec2& velocity);
		static glm::vec2 GetClimateWindVelocity();
		static void SetClimateInitialMoisture(float moistureDepth);
		static float GetClimateInitialMoisture();
		static void SetClimateTemperatureLapseRate(float lapseRate);
		static float GetClimateTemperatureLapseRate();
		static void SetClimateVisualizationMode(ClimateVisualizationMode mode);
		static ClimateVisualizationMode GetClimateVisualizationMode();
		static void RequestClimateReadback();
		static TerrainClimateGPUStatistics GetClimateStatistics();
		static void RequestClimateContractValidation();
		static TerrainClimateGPUValidationResult GetClimateValidationResult();
		static TerrainEnvironmentGPUStatistics GetEnvironmentStatistics();
		static Statistics GetStatistics();
		static bool IntersectsCameraFrustum(
			const glm::vec3& boundsMin,
			const glm::vec3& boundsMax,
			const glm::mat4& transform,
			const glm::mat4& viewProjection);
	};
}
