#pragma once

#include "Glimmer/Scene/Components.h"

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
			uint32_t BoundMaterialTextures = 0;
			float GpuMilliseconds = 0.0f;
			uint64_t GpuTimingSample = 0;
			bool GpuTimingAvailable = false;
			SamplingMode Mode = SamplingMode::AutomaticDistance;
			float DetailDistance = 80.0f;
		};

		static void Init();
		static void Shutdown();
		static void BeginScene();
		static void EndScene();
		static bool Prepare(TerrainComponent& component);
		static void Draw(TerrainComponent& component, const glm::mat4& transform,
			const glm::mat4& viewProjection, const glm::vec3& cameraPosition, int entityID);
		static void Invalidate(TerrainComponent& component);
		static void SetSamplingMode(SamplingMode mode);
		static SamplingMode GetSamplingMode();
		static void SetDetailDistance(float distance);
		static float GetDetailDistance();
		static Statistics GetStatistics();
	};
}
