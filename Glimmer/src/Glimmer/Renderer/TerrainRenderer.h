#pragma once

#include "Glimmer/Scene/Components.h"
#include "Glimmer/Simulation/TerrainHydrologyGPU.h"

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
		static void SetHydrologyVisualizationEnabled(bool enabled);
		static bool IsHydrologyVisualizationEnabled();
		static void RequestHydrologyReadback();
		static TerrainHydrologyGPUStatistics GetHydrologyStatistics();
		static Statistics GetStatistics();
		static bool IntersectsCameraFrustum(
			const glm::vec3& boundsMin,
			const glm::vec3& boundsMax,
			const glm::mat4& transform,
			const glm::mat4& viewProjection);
	};
}
