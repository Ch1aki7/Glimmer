#include "glpch.h"
#include "EnvironmentLighting.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/Cubemap.h"
#include "Glimmer/Renderer/EnvironmentMapLoader.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/TextureCube.h"

#include <unordered_map>

namespace gl {

	namespace {

		struct EnvironmentLightingData
		{
			DiffuseIrradianceSettings IrradianceSettings;
			SpecularPrefilterSettings PrefilterSettings;
			std::unordered_map<
				EnvironmentDerivedMapKey,
				Ref<TextureCube>,
				EnvironmentDerivedMapKeyHash> Cache;
			EnvironmentDerivedMapKey ActiveDiffuseKey;
			EnvironmentDerivedMapKey ActiveSpecularKey;
			Ref<TextureCube> ActiveIrradiance;
			Ref<TextureCube> ActivePrefilter;
			float ActiveIntensity = 0.0f;
			bool HasActiveEnvironment = false;
			EnvironmentLighting::Statistics Stats;
		};

		EnvironmentLightingData s_Data;

		void HashCombine(size_t& seed, uint64_t value)
		{
			seed ^= std::hash<uint64_t>{}(value)
				+ 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
		}

		EnvironmentDerivedMapKey MakeDiffuseKey(
			AssetHandle handle,
			uint64_t version,
			const DiffuseIrradianceSettings& settings)
		{
			return {
				handle,
				version,
				EnvironmentDerivedMapType::DiffuseIrradiance,
				settings.Resolution,
				settings.SampleCount
			};
		}

		EnvironmentDerivedMapKey MakeSpecularKey(
			AssetHandle handle,
			uint64_t version,
			const SpecularPrefilterSettings& settings)
		{
			return {
				handle,
				version,
				EnvironmentDerivedMapType::SpecularPrefilter,
				settings.Resolution,
				settings.SampleCount
			};
		}

		Ref<TextureCube> BuildDiffuseIrradiance(
			const CubemapFloatData& sourceData,
			const DiffuseIrradianceSettings& settings)
		{
			CubemapFloatData irradianceData;
			if (!EnvironmentMapLoader::GenerateDiffuseIrradiance(
				sourceData,
				settings.Resolution,
				settings.SampleCount,
				irradianceData))
			{
				GL_CORE_ERROR("Failed to generate diffuse irradiance Cubemap.");
				return nullptr;
			}

			TextureCubeSpecification specification;
			specification.Size = settings.Resolution;
			specification.MipLevels = 1;
			specification.Format = TextureFormat::RGBA16F;
			specification.ColorSpace = TextureColorSpace::Linear;
			specification.MinFilter = TextureFilter::Linear;
			specification.MagFilter = TextureFilter::Linear;
			Ref<TextureCube> texture = TextureCube::Create(specification);
			if (!texture)
				return nullptr;

			for (uint32_t face = 0; face < irradianceData.Faces.size(); ++face)
			{
				const auto& pixels = irradianceData.Faces[face];
				texture->SetFaceData(
					static_cast<TextureCubeFace>(face),
					pixels.data(),
					static_cast<uint32_t>(pixels.size() * sizeof(float)));
			}
			return texture;
		}

		Ref<TextureCube> BuildSpecularPrefilter(
			const CubemapFloatData& sourceData,
			const SpecularPrefilterSettings& settings)
		{
			CubemapMipChainFloatData prefilterData;
			if (!EnvironmentMapLoader::GenerateSpecularPrefilter(
				sourceData,
				settings.Resolution,
				settings.SampleCount,
				prefilterData))
			{
				GL_CORE_ERROR("Failed to generate specular prefilter Cubemap.");
				return nullptr;
			}

			TextureCubeSpecification specification;
			specification.Size = settings.Resolution;
			specification.MipLevels =
				static_cast<uint32_t>(prefilterData.Mips.size());
			specification.Format = TextureFormat::RGBA16F;
			specification.ColorSpace = TextureColorSpace::Linear;
			specification.MinFilter = TextureFilter::LinearMipmapLinear;
			specification.MagFilter = TextureFilter::Linear;
			Ref<TextureCube> texture = TextureCube::Create(specification);
			if (!texture)
				return nullptr;

			for (uint32_t mip = 0; mip < prefilterData.Mips.size(); ++mip)
			{
				const auto& mipData = prefilterData.Mips[mip];
				for (uint32_t face = 0; face < mipData.Faces.size(); ++face)
				{
					const auto& pixels = mipData.Faces[face];
					texture->SetFaceData(
						static_cast<TextureCubeFace>(face),
						pixels.data(),
						static_cast<uint32_t>(pixels.size() * sizeof(float)),
						mip);
				}
			}
			return texture;
		}

		void ResetActiveEnvironment()
		{
			s_Data.ActiveIrradiance.reset();
			s_Data.ActivePrefilter.reset();
			s_Data.HasActiveEnvironment = false;
		}

	}

	size_t EnvironmentDerivedMapKeyHash::operator()(
		const EnvironmentDerivedMapKey& key) const
	{
		size_t result = 0;
		HashCombine(result, static_cast<uint64_t>(key.SourceHandle));
		HashCombine(result, key.SourceVersion);
		HashCombine(result, static_cast<uint64_t>(key.Type));
		HashCombine(result, key.Resolution);
		HashCombine(result, key.SampleCount);
		return result;
	}

	void EnvironmentLighting::Init()
	{
		s_Data = {};
	}

	void EnvironmentLighting::Shutdown()
	{
		s_Data = {};
	}

	void EnvironmentLighting::SetSkyLight(
		AssetHandle cubemapHandle,
		float intensity,
		bool enabled)
	{
		s_Data.ActiveIntensity = glm::max(intensity, 0.0f);
		if (!enabled || static_cast<uint64_t>(cubemapHandle) == 0)
		{
			ResetActiveEnvironment();
			return;
		}

		const Ref<Cubemap> source = AssetManager::GetCubemap(cubemapHandle);
		if (!source || !source->GetTexture())
		{
			ResetActiveEnvironment();
			return;
		}

		const EnvironmentDerivedMapKey diffuseKey = MakeDiffuseKey(
			cubemapHandle, source->GetVersion(), s_Data.IrradianceSettings);
		const EnvironmentDerivedMapKey specularKey = MakeSpecularKey(
			cubemapHandle, source->GetVersion(), s_Data.PrefilterSettings);
		if (s_Data.HasActiveEnvironment
			&& diffuseKey == s_Data.ActiveDiffuseKey
			&& specularKey == s_Data.ActiveSpecularKey
			&& s_Data.ActiveIrradiance
			&& s_Data.ActivePrefilter)
			return;

		for (auto iterator = s_Data.Cache.begin();
			iterator != s_Data.Cache.end();)
		{
			if (iterator->first.SourceHandle == cubemapHandle
				&& iterator->first.SourceVersion != source->GetVersion())
				iterator = s_Data.Cache.erase(iterator);
			else
				++iterator;
		}

		auto diffuse = s_Data.Cache.find(diffuseKey);
		auto specular = s_Data.Cache.find(specularKey);
		if (diffuse != s_Data.Cache.end())
		{
			s_Data.ActiveIrradiance = diffuse->second;
			++s_Data.Stats.CacheHits;
		}
		if (specular != s_Data.Cache.end())
		{
			s_Data.ActivePrefilter = specular->second;
			++s_Data.Stats.CacheHits;
		}

		const bool needsDiffuse = diffuse == s_Data.Cache.end();
		const bool needsSpecular = specular == s_Data.Cache.end();
		s_Data.Stats.CacheMisses +=
			static_cast<uint64_t>(needsDiffuse)
			+ static_cast<uint64_t>(needsSpecular);
		if (needsDiffuse || needsSpecular)
		{
			CubemapFloatData sourceData;
			if (!EnvironmentMapLoader::ReadTextureCube(
				*source->GetTexture(), sourceData))
			{
				GL_CORE_ERROR(
					"Failed to read source Cubemap for environment lighting.");
				ResetActiveEnvironment();
				return;
			}

			if (needsDiffuse)
			{
				s_Data.ActiveIrradiance = BuildDiffuseIrradiance(
					sourceData, s_Data.IrradianceSettings);
				if (s_Data.ActiveIrradiance)
				{
					s_Data.Cache.emplace(
						diffuseKey, s_Data.ActiveIrradiance);
					++s_Data.Stats.GenerationCount;
					++s_Data.Stats.DiffuseGenerationCount;
					GL_CORE_INFO(
						"Diffuse irradiance generated: handle={0}, version={1}, "
						"resolution={2}, samples={3}",
						static_cast<uint64_t>(cubemapHandle),
						source->GetVersion(),
						s_Data.IrradianceSettings.Resolution,
						s_Data.IrradianceSettings.SampleCount);
				}
			}
			if (needsSpecular)
			{
				s_Data.ActivePrefilter = BuildSpecularPrefilter(
					sourceData, s_Data.PrefilterSettings);
				if (s_Data.ActivePrefilter)
				{
					s_Data.Cache.emplace(
						specularKey, s_Data.ActivePrefilter);
					++s_Data.Stats.GenerationCount;
					++s_Data.Stats.SpecularGenerationCount;
					GL_CORE_INFO(
						"Specular prefilter generated: handle={0}, version={1}, "
						"resolution={2}, mips={3}, samples={4}",
						static_cast<uint64_t>(cubemapHandle),
						source->GetVersion(),
						s_Data.PrefilterSettings.Resolution,
						s_Data.ActivePrefilter->GetSpecification().MipLevels,
						s_Data.PrefilterSettings.SampleCount);
				}
			}
		}

		s_Data.ActiveDiffuseKey = diffuseKey;
		s_Data.ActiveSpecularKey = specularKey;
		s_Data.HasActiveEnvironment =
			s_Data.ActiveIrradiance && s_Data.ActivePrefilter;
		s_Data.Stats.CacheEntries =
			static_cast<uint32_t>(s_Data.Cache.size());
	}

	void EnvironmentLighting::BindForLighting(
		const Ref<Shader>& shader,
		uint32_t diffuseTextureSlot,
		uint32_t specularTextureSlot)
	{
		if (!shader)
			return;

		const bool diffuseAvailable =
			s_Data.ActiveIrradiance && s_Data.ActiveIntensity > 0.0f;
		const bool specularAvailable =
			s_Data.ActivePrefilter && s_Data.ActiveIntensity > 0.0f;
		shader->UploadUniformInt(
			"u_HasDiffuseIrradiance", diffuseAvailable ? 1 : 0);
		shader->UploadUniformInt(
			"u_DiffuseIrradianceMap",
			static_cast<int>(diffuseTextureSlot));
		if (diffuseAvailable)
			s_Data.ActiveIrradiance->Bind(diffuseTextureSlot);

		shader->UploadUniformInt(
			"u_HasSpecularPrefilter", specularAvailable ? 1 : 0);
		shader->UploadUniformInt(
			"u_SpecularPrefilterMap",
			static_cast<int>(specularTextureSlot));
		const float maxLod = specularAvailable
			? static_cast<float>(
				s_Data.ActivePrefilter->GetSpecification().MipLevels - 1)
			: 0.0f;
		shader->UploadUniformFloat(
			"u_SpecularPrefilterMaxLod", maxLod);
		if (specularAvailable)
			s_Data.ActivePrefilter->Bind(specularTextureSlot);

		shader->UploadUniformFloat(
			"u_SkyLightIntensity",
			(diffuseAvailable || specularAvailable)
				? s_Data.ActiveIntensity : 0.0f);
	}

	void EnvironmentLighting::SetIrradianceSettings(
		const DiffuseIrradianceSettings& settings)
	{
		DiffuseIrradianceSettings sanitized;
		sanitized.Resolution = std::clamp(settings.Resolution, 4u, 128u);
		sanitized.SampleCount = std::clamp(settings.SampleCount, 16u, 4096u);
		if (sanitized == s_Data.IrradianceSettings)
			return;
		s_Data.IrradianceSettings = sanitized;
		ResetActiveEnvironment();
	}

	DiffuseIrradianceSettings EnvironmentLighting::GetIrradianceSettings()
	{
		return s_Data.IrradianceSettings;
	}

	void EnvironmentLighting::SetPrefilterSettings(
		const SpecularPrefilterSettings& settings)
	{
		SpecularPrefilterSettings sanitized;
		sanitized.Resolution = std::clamp(settings.Resolution, 8u, 256u);
		sanitized.SampleCount = std::clamp(settings.SampleCount, 16u, 4096u);
		if (sanitized == s_Data.PrefilterSettings)
			return;
		s_Data.PrefilterSettings = sanitized;
		ResetActiveEnvironment();
	}

	SpecularPrefilterSettings EnvironmentLighting::GetPrefilterSettings()
	{
		return s_Data.PrefilterSettings;
	}

	EnvironmentLighting::Statistics EnvironmentLighting::GetStatistics()
	{
		Statistics statistics = s_Data.Stats;
		statistics.CacheEntries = static_cast<uint32_t>(s_Data.Cache.size());
		return statistics;
	}

}
