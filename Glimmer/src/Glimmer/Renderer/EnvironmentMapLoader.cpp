#include "glpch.h"
#include "EnvironmentMapLoader.h"

#include "stb_image.h"

#include <cmath>

namespace gl {
	namespace {
		constexpr float Pi = 3.14159265358979323846f;

		float SRGBToLinear(float value)
		{
			value = glm::max(value, 0.0f);
			return value <= 0.04045f
				? value / 12.92f
				: std::pow((value + 0.055f) / 1.055f, 2.4f);
		}

		float RadicalInverse(uint32_t bits)
		{
			bits = (bits << 16u) | (bits >> 16u);
			bits = ((bits & 0x55555555u) << 1u)
				| ((bits & 0xAAAAAAAAu) >> 1u);
			bits = ((bits & 0x33333333u) << 2u)
				| ((bits & 0xCCCCCCCCu) >> 2u);
			bits = ((bits & 0x0F0F0F0Fu) << 4u)
				| ((bits & 0xF0F0F0F0u) >> 4u);
			bits = ((bits & 0x00FF00FFu) << 8u)
				| ((bits & 0xFF00FF00u) >> 8u);
			return static_cast<float>(bits) * 2.3283064365386963e-10f;
		}

		glm::vec4 ReadPixel(
			const FloatImageData& image,
			uint32_t x,
			uint32_t y)
		{
			const size_t offset =
				(static_cast<size_t>(y) * image.Width + x) * 4;
			return {
				image.Pixels[offset + 0],
				image.Pixels[offset + 1],
				image.Pixels[offset + 2],
				image.Pixels[offset + 3]
			};
		}

		glm::vec4 SampleEquirectangular(
			const FloatImageData& image,
			const glm::vec3& direction)
		{
			const glm::vec3 normalized = glm::normalize(direction);
			const float longitude = std::atan2(normalized.z, normalized.x);
			const float latitude = std::acos(glm::clamp(
				normalized.y, -1.0f, 1.0f));
			const float imageX = (longitude / (2.0f * Pi) + 0.5f)
				* static_cast<float>(image.Width) - 0.5f;
			const float imageY = glm::clamp(
				latitude / Pi * static_cast<float>(image.Height) - 0.5f,
				0.0f, static_cast<float>(image.Height - 1));

			const int x0Unwrapped = static_cast<int>(std::floor(imageX));
			const int y0 = static_cast<int>(std::floor(imageY));
			const int y1 = std::min(
				y0 + 1, static_cast<int>(image.Height) - 1);
			const auto wrapX = [&image](int x)
			{
				const int width = static_cast<int>(image.Width);
				return static_cast<uint32_t>((x % width + width) % width);
			};
			const uint32_t x0 = wrapX(x0Unwrapped);
			const uint32_t x1 = wrapX(x0Unwrapped + 1);
			const float blendX = imageX - std::floor(imageX);
			const float blendY = imageY - std::floor(imageY);

			const glm::vec4 top = glm::mix(
				ReadPixel(image, x0, static_cast<uint32_t>(y0)),
				ReadPixel(image, x1, static_cast<uint32_t>(y0)),
				blendX);
			const glm::vec4 bottom = glm::mix(
				ReadPixel(image, x0, static_cast<uint32_t>(y1)),
				ReadPixel(image, x1, static_cast<uint32_t>(y1)),
				blendX);
			return glm::mix(top, bottom, blendY);
		}

		TextureCubeFace DirectionToFace(
			const glm::vec3& direction,
			float& coordinateX,
			float& coordinateY)
		{
			const glm::vec3 absolute = glm::abs(direction);
			if (absolute.x >= absolute.y && absolute.x >= absolute.z)
			{
				if (direction.x >= 0.0f)
				{
					coordinateX = -direction.z / absolute.x;
					coordinateY = -direction.y / absolute.x;
					return TextureCubeFace::PositiveX;
				}
				coordinateX = direction.z / absolute.x;
				coordinateY = -direction.y / absolute.x;
				return TextureCubeFace::NegativeX;
			}
			if (absolute.y >= absolute.z)
			{
				if (direction.y >= 0.0f)
				{
					coordinateX = direction.x / absolute.y;
					coordinateY = direction.z / absolute.y;
					return TextureCubeFace::PositiveY;
				}
				coordinateX = direction.x / absolute.y;
				coordinateY = -direction.z / absolute.y;
				return TextureCubeFace::NegativeY;
			}
			if (direction.z >= 0.0f)
			{
				coordinateX = direction.x / absolute.z;
				coordinateY = -direction.y / absolute.z;
				return TextureCubeFace::PositiveZ;
			}
			coordinateX = -direction.x / absolute.z;
			coordinateY = -direction.y / absolute.z;
			return TextureCubeFace::NegativeZ;
		}

		glm::vec4 SampleCubemapUnchecked(
			const CubemapFloatData& cubemap,
			const glm::vec3& normalizedDirection)
		{
			float coordinateX = 0.0f;
			float coordinateY = 0.0f;
			const TextureCubeFace face = DirectionToFace(
				normalizedDirection, coordinateX, coordinateY);
			const float imageX = glm::clamp(
				(coordinateX * 0.5f + 0.5f) * cubemap.Size - 0.5f,
				0.0f, static_cast<float>(cubemap.Size - 1));
			const float imageY = glm::clamp(
				(coordinateY * 0.5f + 0.5f) * cubemap.Size - 0.5f,
				0.0f, static_cast<float>(cubemap.Size - 1));
			const uint32_t x0 = static_cast<uint32_t>(std::floor(imageX));
			const uint32_t y0 = static_cast<uint32_t>(std::floor(imageY));
			const uint32_t x1 = std::min(x0 + 1, cubemap.Size - 1);
			const uint32_t y1 = std::min(y0 + 1, cubemap.Size - 1);
			const float blendX = imageX - std::floor(imageX);
			const float blendY = imageY - std::floor(imageY);
			const auto& pixels = cubemap.Faces[static_cast<uint32_t>(face)];
			const auto read = [&pixels, &cubemap](uint32_t x, uint32_t y)
			{
				const size_t offset =
					(static_cast<size_t>(y) * cubemap.Size + x) * 4;
				return glm::vec4(
					pixels[offset + 0], pixels[offset + 1],
					pixels[offset + 2], pixels[offset + 3]);
			};
			return glm::mix(
				glm::mix(read(x0, y0), read(x1, y0), blendX),
				glm::mix(read(x0, y1), read(x1, y1), blendX),
				blendY);
		}
	}

	bool CubemapFloatData::IsValid() const
	{
		if (Size == 0)
			return false;
		const size_t expected =
			static_cast<size_t>(Size) * Size * 4;
		for (const auto& face : Faces)
		{
			if (face.size() != expected)
				return false;
		}
		return true;
	}

	bool EnvironmentMapLoader::LoadEquirectangularHDR(
		const std::filesystem::path& path,
		FloatImageData& image)
	{
		image = {};
		stbi_set_flip_vertically_on_load(0);
		int width = 0;
		int height = 0;
		int channels = 0;
		float* pixels = stbi_loadf(
			path.string().c_str(),
			&width,
			&height,
			&channels,
			STBI_rgb_alpha);
		if (!pixels)
		{
			GL_CORE_ERROR("Failed to load HDR environment: {0}", path.string());
			return false;
		}
		if (width < 2 || height < 2)
		{
			GL_CORE_ERROR(
				"HDR environment must be at least 2x2: {0}",
				path.string());
			stbi_image_free(pixels);
			return false;
		}

		image.Width = static_cast<uint32_t>(width);
		image.Height = static_cast<uint32_t>(height);
		const size_t componentCount =
			static_cast<size_t>(image.Width) * image.Height * 4;
		image.Pixels.assign(pixels, pixels + componentCount);
		stbi_image_free(pixels);

		if (image.Width != image.Height * 2)
		{
			GL_CORE_WARN(
				"HDR environment is not 2:1; conversion will use the full image: {0}",
				path.string());
		}
		return true;
	}

	uint32_t EnvironmentMapLoader::SuggestFaceSize(
		const FloatImageData& source)
	{
		if (!source.IsValid())
			return 0;
		return std::max(
			1u, std::min(source.Width / 4, source.Height / 2));
	}

	glm::vec3 EnvironmentMapLoader::CubemapDirection(
		TextureCubeFace face,
		float coordinateX,
		float coordinateY)
	{
		switch (face)
		{
		case TextureCubeFace::PositiveX:
			return glm::normalize(glm::vec3(
				1.0f, -coordinateY, -coordinateX));
		case TextureCubeFace::NegativeX:
			return glm::normalize(glm::vec3(
				-1.0f, -coordinateY, coordinateX));
		case TextureCubeFace::PositiveY:
			return glm::normalize(glm::vec3(
				coordinateX, 1.0f, coordinateY));
		case TextureCubeFace::NegativeY:
			return glm::normalize(glm::vec3(
				coordinateX, -1.0f, -coordinateY));
		case TextureCubeFace::PositiveZ:
			return glm::normalize(glm::vec3(
				coordinateX, -coordinateY, 1.0f));
		case TextureCubeFace::NegativeZ:
			return glm::normalize(glm::vec3(
				-coordinateX, -coordinateY, -1.0f));
		}
		return { 0.0f, 0.0f, 1.0f };
	}

	bool EnvironmentMapLoader::ConvertEquirectangularToCubemap(
		const FloatImageData& source,
		uint32_t faceSize,
		CubemapFloatData& cubemap)
	{
		cubemap = {};
		if (!source.IsValid() || faceSize == 0)
			return false;

		cubemap.Size = faceSize;
		const size_t componentCount =
			static_cast<size_t>(faceSize) * faceSize * 4;
		for (uint32_t faceIndex = 0; faceIndex < cubemap.Faces.size();
			++faceIndex)
		{
			auto& destination = cubemap.Faces[faceIndex];
			destination.resize(componentCount);
			for (uint32_t y = 0; y < faceSize; ++y)
			{
				for (uint32_t x = 0; x < faceSize; ++x)
				{
					const float coordinateX =
						2.0f * (static_cast<float>(x) + 0.5f)
						/ static_cast<float>(faceSize) - 1.0f;
					const float coordinateY =
						2.0f * (static_cast<float>(y) + 0.5f)
						/ static_cast<float>(faceSize) - 1.0f;
					const glm::vec4 sample = SampleEquirectangular(
						source,
						CubemapDirection(
							static_cast<TextureCubeFace>(faceIndex),
							coordinateX,
							coordinateY));
					const size_t offset =
						(static_cast<size_t>(y) * faceSize + x) * 4;
					destination[offset + 0] = sample.r;
					destination[offset + 1] = sample.g;
					destination[offset + 2] = sample.b;
					destination[offset + 3] = sample.a;
				}
			}
		}
		return true;
	}

	bool EnvironmentMapLoader::ReadTextureCube(
		const TextureCube& texture,
		CubemapFloatData& cubemap)
	{
		cubemap = {};
		const TextureCubeSpecification& specification =
			texture.GetSpecification();
		if (specification.Size == 0)
			return false;

		cubemap.Size = specification.Size;
		const uint32_t componentCount =
			specification.Size * specification.Size * 4;
		for (uint32_t face = 0; face < cubemap.Faces.size(); ++face)
		{
			auto& pixels = cubemap.Faces[face];
			pixels.resize(componentCount);
			if (!texture.GetFaceFloatData(
				static_cast<TextureCubeFace>(face),
				pixels.data(), componentCount))
			{
				cubemap = {};
				return false;
			}
			if (specification.ColorSpace == TextureColorSpace::SRGB)
			{
				for (size_t offset = 0; offset < pixels.size(); offset += 4)
				{
					pixels[offset + 0] = SRGBToLinear(pixels[offset + 0]);
					pixels[offset + 1] = SRGBToLinear(pixels[offset + 1]);
					pixels[offset + 2] = SRGBToLinear(pixels[offset + 2]);
				}
			}
		}
		return true;
	}

	glm::vec4 EnvironmentMapLoader::SampleCubemap(
		const CubemapFloatData& cubemap,
		const glm::vec3& direction)
	{
		if (!cubemap.IsValid() || glm::dot(direction, direction) < 0.000001f)
			return glm::vec4(0.0f);
		return SampleCubemapUnchecked(cubemap, glm::normalize(direction));
	}

	bool EnvironmentMapLoader::GenerateDiffuseIrradiance(
		const CubemapFloatData& source,
		uint32_t faceSize,
		uint32_t sampleCount,
		CubemapFloatData& irradiance)
	{
		irradiance = {};
		if (!source.IsValid() || faceSize == 0 || sampleCount == 0)
			return false;

		irradiance.Size = faceSize;
		const size_t componentCount =
			static_cast<size_t>(faceSize) * faceSize * 4;
		for (uint32_t face = 0; face < irradiance.Faces.size(); ++face)
		{
			auto& destination = irradiance.Faces[face];
			destination.resize(componentCount);
			for (uint32_t y = 0; y < faceSize; ++y)
			{
				for (uint32_t x = 0; x < faceSize; ++x)
				{
					const float coordinateX =
						2.0f * (static_cast<float>(x) + 0.5f) / faceSize - 1.0f;
					const float coordinateY =
						2.0f * (static_cast<float>(y) + 0.5f) / faceSize - 1.0f;
					const glm::vec3 normal = CubemapDirection(
						static_cast<TextureCubeFace>(face),
						coordinateX, coordinateY);
					const glm::vec3 up = std::abs(normal.z) < 0.999f
						? glm::vec3(0.0f, 0.0f, 1.0f)
						: glm::vec3(1.0f, 0.0f, 0.0f);
					const glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
					const glm::vec3 bitangent = glm::cross(normal, tangent);
					glm::vec3 accumulated(0.0f);
					for (uint32_t sample = 0; sample < sampleCount; ++sample)
					{
						const float sequenceX =
							(static_cast<float>(sample) + 0.5f) / sampleCount;
						const float sequenceY = RadicalInverse(sample);
						const float phi = 2.0f * Pi * sequenceX;
						const float sinTheta = std::sqrt(sequenceY);
						const float cosTheta = std::sqrt(1.0f - sequenceY);
						const glm::vec3 local(
							std::cos(phi) * sinTheta,
							std::sin(phi) * sinTheta,
							cosTheta);
						const glm::vec3 sampleDirection = glm::normalize(
							tangent * local.x + bitangent * local.y
							+ normal * local.z);
						accumulated += glm::vec3(
							SampleCubemapUnchecked(source, sampleDirection));
					}
					const glm::vec3 value =
						accumulated * (Pi / static_cast<float>(sampleCount));
					const size_t offset =
						(static_cast<size_t>(y) * faceSize + x) * 4;
					destination[offset + 0] = value.r;
					destination[offset + 1] = value.g;
					destination[offset + 2] = value.b;
					destination[offset + 3] = 1.0f;
				}
			}
		}
		return true;
	}
}
