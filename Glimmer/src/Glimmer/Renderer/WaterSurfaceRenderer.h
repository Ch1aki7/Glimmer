#pragma once
#include "Glimmer/Renderer/Framebuffer.h"
#include "Glimmer/Scene/Components.h"
#include <vector>

namespace gl {
	struct WaterSurfaceSettings
	{
		bool Enabled = true;
		float Absorption = 1.0f;
		float RefractionPixels = 5.0f;
		float FoamStrength = 0.35f;
		float SedimentTint = 0.4f;
		float ShoreWetness = 0.6f;
	};

	struct WaterSurfaceInstance
	{
		const TerrainComponent* Terrain = nullptr;
		glm::mat4 Transform{ 1.0f };
		int EntityID = -1;
	};

	class WaterSurfaceRenderer
	{
	public:
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint64_t Triangles = 0;
			bool SnapshotReady = false;
		};
		static void Render(const Ref<Framebuffer>& target,
			const std::vector<WaterSurfaceInstance>& instances,
			const glm::mat4& viewProjection, const glm::vec3& cameraPosition);
		static void Shutdown();
		static void SetSettings(const WaterSurfaceSettings& settings);
		static const WaterSurfaceSettings& GetSettings();
		static Statistics GetStatistics();
	};
}
