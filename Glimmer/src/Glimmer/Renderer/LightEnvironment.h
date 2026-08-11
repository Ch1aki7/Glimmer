#pragma once

#include "Glimmer/Asset/Asset.h"

#include <glm/glm.hpp>
#include <vector>

namespace gl {

	struct DirectionalLight
	{
		glm::vec3 Direction{ 0.0f, -1.0f, 0.0f };
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		float AmbientIntensity = 0.05f;
		bool Enabled = false;
	};

	struct PointLight
	{
		glm::vec3 Position{ 0.0f };
		glm::vec3 Color{ 1.0f };
		float Intensity = 1.0f;
		float Range = 10.0f;
	};

	struct LightEnvironment
	{
		static constexpr uint32_t MaxPointLights = 16;

		DirectionalLight Directional;
		std::vector<PointLight> PointLights;
		AssetHandle SkyLightCubemap{ 0 };
		float SkyLightIntensity = 0.0f;
		bool SkyLightEnabled = false;
	};

}
