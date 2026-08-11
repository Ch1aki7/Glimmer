#pragma once

#include "Glimmer/Asset/Asset.h"
#include "Glimmer/Core/Core.h"

#include <cstddef>
#include <cstdint>

namespace gl {

	class Shader;
	class TextureCube;

	struct DiffuseIrradianceSettings
	{
		uint32_t Resolution = 32;
		uint32_t SampleCount = 64;

		bool operator==(const DiffuseIrradianceSettings& other) const
		{
			return Resolution == other.Resolution
				&& SampleCount == other.SampleCount;
		}
	};

	struct SpecularPrefilterSettings
	{
		uint32_t Resolution = 64;
		uint32_t SampleCount = 64;

		bool operator==(const SpecularPrefilterSettings& other) const
		{
			return Resolution == other.Resolution
				&& SampleCount == other.SampleCount;
		}
	};

	enum class EnvironmentDerivedMapType : uint8_t
	{
		DiffuseIrradiance = 0,
		SpecularPrefilter
	};

	struct EnvironmentDerivedMapKey
	{
		AssetHandle SourceHandle{ 0 };
		uint64_t SourceVersion = 0;
		EnvironmentDerivedMapType Type =
			EnvironmentDerivedMapType::DiffuseIrradiance;
		uint32_t Resolution = 0;
		uint32_t SampleCount = 0;

		bool operator==(const EnvironmentDerivedMapKey& other) const
		{
			return SourceHandle == other.SourceHandle
				&& SourceVersion == other.SourceVersion
				&& Type == other.Type
				&& Resolution == other.Resolution
				&& SampleCount == other.SampleCount;
		}
	};

	struct EnvironmentDerivedMapKeyHash
	{
		size_t operator()(const EnvironmentDerivedMapKey& key) const;
	};

	class EnvironmentLighting
	{
	public:
		struct Statistics
		{
			uint64_t CacheHits = 0;
			uint64_t CacheMisses = 0;
			uint64_t GenerationCount = 0;
			uint64_t DiffuseGenerationCount = 0;
			uint64_t SpecularGenerationCount = 0;
			uint32_t CacheEntries = 0;
		};

		static void Init();
		static void Shutdown();
		static void SetSkyLight(
			AssetHandle cubemapHandle,
			float intensity,
			bool enabled);
		static void BindForLighting(
			const Ref<Shader>& shader,
			uint32_t diffuseTextureSlot,
			uint32_t specularTextureSlot);

		static void SetIrradianceSettings(
			const DiffuseIrradianceSettings& settings);
		static DiffuseIrradianceSettings GetIrradianceSettings();
		static void SetPrefilterSettings(
			const SpecularPrefilterSettings& settings);
		static SpecularPrefilterSettings GetPrefilterSettings();
		static Statistics GetStatistics();
	};

}
