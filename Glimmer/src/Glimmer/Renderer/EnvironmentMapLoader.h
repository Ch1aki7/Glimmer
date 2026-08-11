#pragma once

#include "Glimmer/Renderer/TextureCube.h"

#include <array>
#include <filesystem>
#include <vector>

namespace gl {

	struct FloatImageData
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		std::vector<float> Pixels;

		bool IsValid() const
		{
			return Width > 0 && Height > 0
				&& Pixels.size() == static_cast<size_t>(Width) * Height * 4;
		}
	};

	struct CubemapFloatData
	{
		uint32_t Size = 0;
		std::array<std::vector<float>, 6> Faces;

		bool IsValid() const;
	};

	class EnvironmentMapLoader
	{
	public:
		static bool LoadEquirectangularHDR(
			const std::filesystem::path& path,
			FloatImageData& image);
		static bool ConvertEquirectangularToCubemap(
			const FloatImageData& source,
			uint32_t faceSize,
			CubemapFloatData& cubemap);
		static bool ReadTextureCube(
			const TextureCube& texture,
			CubemapFloatData& cubemap);
		static glm::vec4 SampleCubemap(
			const CubemapFloatData& cubemap,
			const glm::vec3& direction);
		static bool GenerateDiffuseIrradiance(
			const CubemapFloatData& source,
			uint32_t faceSize,
			uint32_t sampleCount,
			CubemapFloatData& irradiance);
		static uint32_t SuggestFaceSize(const FloatImageData& source);
		static glm::vec3 CubemapDirection(
			TextureCubeFace face,
			float coordinateX,
			float coordinateY);
	};
}
