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

		struct DerivedEnvironment
		{
			Ref<TextureCube> DiffuseIrradiance;
		};

		struct EnvironmentLightingData
		{
			DiffuseIrradianceSettings Settings;
			std::unordered_map<
				EnvironmentDerivedMapKey,
				DerivedEnvironment,
				EnvironmentDerivedMapKeyHash> Cache;
			EnvironmentDerivedMapKey ActiveKey;
			Ref<TextureCube> ActiveIrradiance;
			float ActiveIntensity = 0.0f;
			bool HasActiveKey = false;
			EnvironmentLighting::Statistics Stats;
		};

		EnvironmentLightingData s_Data;

		void HashCombine(size_t& seed, uint64_t value)
		{
			seed ^= std::hash<uint64_t>{}(value)
				+ 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
		}

		Ref<TextureCube> BuildDiffuseIrradiance(
			const Ref<Cubemap>& source,
			const DiffuseIrradianceSettings& settings)
		{
			if (!source || !source->GetTexture())
				return nullptr;

			CubemapFloatData sourceData;
			if (!EnvironmentMapLoader::ReadTextureCube(
				*source->GetTexture(), sourceData))
			{
				GL_CORE_ERROR("Failed to read source Cubemap for diffuse irradiance.");
				return nullptr;
			}

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

	}

	size_t EnvironmentDerivedMapKeyHash::operator()(
		const EnvironmentDerivedMapKey& key) const
	{
		size_t result = 0;
		HashCombine(result, static_cast<uint64_t>(key.SourceHandle));
		HashCombine(result, key.SourceVersion);
		HashCombine(result, key.Irradiance.Resolution);
		HashCombine(result, key.Irradiance.SampleCount);
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
			s_Data.ActiveIrradiance.reset();
			s_Data.HasActiveKey = false;
			return;
		}

		const Ref<Cubemap> source = AssetManager::GetCubemap(cubemapHandle);
		if (!source || !source->GetTexture())
		{
			s_Data.ActiveIrradiance.reset();
			s_Data.HasActiveKey = false;
			return;
		}

		EnvironmentDerivedMapKey key;
		key.SourceHandle = cubemapHandle;
		key.SourceVersion = source->GetVersion();
		key.Irradiance = s_Data.Settings;
		if (s_Data.HasActiveKey && key == s_Data.ActiveKey
			&& s_Data.ActiveIrradiance)
			return;

		const auto cached = s_Data.Cache.find(key);
		if (cached != s_Data.Cache.end())
		{
			s_Data.ActiveKey = key;
			s_Data.ActiveIrradiance = cached->second.DiffuseIrradiance;
			s_Data.HasActiveKey = true;
			++s_Data.Stats.CacheHits;
			s_Data.Stats.CacheEntries =
				static_cast<uint32_t>(s_Data.Cache.size());
			return;
		}

		++s_Data.Stats.CacheMisses;
		Ref<TextureCube> irradiance =
			BuildDiffuseIrradiance(source, s_Data.Settings);
		if (!irradiance)
		{
			s_Data.ActiveIrradiance.reset();
			s_Data.HasActiveKey = false;
			return;
		}

		for (auto iterator = s_Data.Cache.begin();
			iterator != s_Data.Cache.end();)
		{
			if (iterator->first.SourceHandle == cubemapHandle)
				iterator = s_Data.Cache.erase(iterator);
			else
				++iterator;
		}
		s_Data.Cache.emplace(key, DerivedEnvironment{ irradiance });
		s_Data.ActiveKey = key;
		s_Data.ActiveIrradiance = std::move(irradiance);
		s_Data.HasActiveKey = true;
		++s_Data.Stats.GenerationCount;
		s_Data.Stats.CacheEntries =
			static_cast<uint32_t>(s_Data.Cache.size());
		GL_CORE_INFO(
			"Diffuse irradiance generated: handle={0}, version={1}, "
			"resolution={2}, samples={3}",
			static_cast<uint64_t>(cubemapHandle),
			source->GetVersion(),
			s_Data.Settings.Resolution,
			s_Data.Settings.SampleCount);
	}

	void EnvironmentLighting::BindForLighting(
		const Ref<Shader>& shader,
		uint32_t textureSlot)
	{
		if (!shader)
			return;
		const bool available =
			s_Data.ActiveIrradiance && s_Data.ActiveIntensity > 0.0f;
		shader->UploadUniformInt(
			"u_HasDiffuseIrradiance", available ? 1 : 0);
		shader->UploadUniformFloat(
			"u_SkyLightIntensity", available ? s_Data.ActiveIntensity : 0.0f);
		shader->UploadUniformInt(
			"u_DiffuseIrradianceMap", static_cast<int>(textureSlot));
		if (available)
			s_Data.ActiveIrradiance->Bind(textureSlot);
	}

	void EnvironmentLighting::SetIrradianceSettings(
		const DiffuseIrradianceSettings& settings)
	{
		DiffuseIrradianceSettings sanitized;
		sanitized.Resolution = std::clamp(settings.Resolution, 4u, 128u);
		sanitized.SampleCount = std::clamp(settings.SampleCount, 16u, 4096u);
		if (sanitized == s_Data.Settings)
			return;
		s_Data.Settings = sanitized;
		s_Data.ActiveIrradiance.reset();
		s_Data.HasActiveKey = false;
	}

	DiffuseIrradianceSettings EnvironmentLighting::GetIrradianceSettings()
	{
		return s_Data.Settings;
	}

	EnvironmentLighting::Statistics EnvironmentLighting::GetStatistics()
	{
		Statistics statistics = s_Data.Stats;
		statistics.CacheEntries = static_cast<uint32_t>(s_Data.Cache.size());
		return statistics;
	}

}
