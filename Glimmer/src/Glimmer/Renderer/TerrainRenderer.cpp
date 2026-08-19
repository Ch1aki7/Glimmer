#include "glpch.h"
#include "TerrainRenderer.h"
#include "EnvironmentLighting.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/ShadowRenderer.h"
#include "Glimmer/Renderer/GPUTimer.h"
#include "Glimmer/Renderer/FrustumCulling.h"
#include "Glimmer/Terrain/Terrain.h"
#include "Glimmer/Terrain/TerrainChunkLayout.h"
#include "Glimmer/Terrain/TerrainMaterial.h"

#include <cstdlib>

namespace gl {
	namespace {
		struct TerrainRendererData
		{
			Ref<GPUTimer> Timer;
			TerrainRenderer::Statistics Stats;
			TerrainRenderer::SamplingMode Sampling =
				TerrainRenderer::SamplingMode::FullFourLayers;
			float DetailDistance = 80.0f;
			float LODMiddleDistance = 90.0f;
			float LODFarDistance = 180.0f;
			float LODHysteresis = 5.0f;
			bool VisualizeLODs = false;
			float LastGpuMilliseconds = 0.0f;
			uint64_t GpuTimingSample = 0;
			bool HasGpuTiming = false;
			bool PassActive = false;
			bool HydrologyPlaying = false;
			TerrainRenderer::HydrologyVisualizationMode HydrologyVisualization =
				TerrainRenderer::HydrologyVisualizationMode::None;
			bool HydrologyReadbackRequested = false;
			bool HydrologyValidationRequested = false;
			bool HydrologyEnvironmentValidationChecked = false;
			float HydrologyRainfall = 0.02f;
			float HydrologySedimentSeedDensity = 1.0f;
			float HydrologySedimentCapacityScale = 1.0f;
			float HydrologyErosionRate = 0.0f;
			float HydrologyDepositionRate = 0.0f;
			float HydrologyTerrainDensity = 1.0f;
			float HydrologyMaximumErosionDepth = 1.0f;
			float HydrologyMaximumHeightChange = 0.01f;
			float DeltaSeconds = 0.0f;
			uint64_t HydrologyResetRequest = 0;
			uint64_t HydrologySingleStepRequest = 0;
			uint64_t HydrologySedimentSeedRequest = 0;
			TerrainHydrologyGPUStatistics HydrologyStats;
			TerrainHydrologyGPUValidationResult HydrologyValidation;
			bool ClimatePlaying = false;
			TerrainRenderer::ClimateVisualizationMode ClimateVisualization =
				TerrainRenderer::ClimateVisualizationMode::None;
			bool ClimateReadbackRequested = false;
			bool ClimateValidationRequested = false;
			bool ClimateEnvironmentValidationChecked = false;
			glm::vec2 ClimateWindVelocity = { 1.0f, 0.0f };
			float ClimateInitialMoisture = 0.01f;
			float ClimateTemperatureLapseRate = 0.0065f;
			uint64_t ClimateResetRequest = 0;
			uint64_t ClimateSingleStepRequest = 0;
			TerrainClimateGPUStatistics ClimateStats;
			TerrainClimateGPUValidationResult ClimateValidation;
			TerrainEnvironmentGPUStatistics EnvironmentStats;
			uint64_t FrameSerial = 0;
			bool HydrologyFrameActive = false;
		};

		TerrainRendererData s_Data;

		SimulationGridSpecification CreateGridSpecification(uint32_t resolution)
		{
			SimulationGridSpecification specification;
			specification.Width = resolution;
			specification.Height = resolution;
			specification.Format = TextureFormat::R32F;
			specification.Filter = TextureFilter::Linear;
			specification.Wrap = TextureWrap::ClampToEdge;
			return specification;
		}

		std::filesystem::path ResolveComputePath(
			AssetHandle handle,
			const std::filesystem::path& generationPath,
			const char* fallbackFileName)
		{
			const std::filesystem::path registeredPath =
				AssetManager::GetFileSystemPath(handle);
			if (!registeredPath.empty())
				return registeredPath;
			return generationPath.parent_path() / fallbackFileName;
		}

		bool ShouldValidateTerrain()
		{
#ifdef GL_PLATFORM_WINDOWS
			char* value = nullptr;
			size_t length = 0;
			const bool enabled = _dupenv_s(&value, &length,
				"GLIMMER_TERRAIN_VALIDATE") == 0
				&& value != nullptr && std::string(value) == "1";
			std::free(value);
			return enabled;
#else
			const char* value = std::getenv("GLIMMER_TERRAIN_VALIDATE");
			return value && std::string(value) == "1";
#endif
		}

		bool ShouldValidateHydrology()
		{
#ifdef GL_PLATFORM_WINDOWS
			char* value = nullptr;
			size_t length = 0;
			const bool enabled = _dupenv_s(&value, &length,
				"GLIMMER_HYDROLOGY_VALIDATE") == 0
				&& value != nullptr && std::string(value) == "1";
			std::free(value);
			return enabled;
#else
			const char* value = std::getenv("GLIMMER_HYDROLOGY_VALIDATE");
			return value && std::string(value) == "1";
#endif
		}

		bool ShouldValidateClimate()
		{
#ifdef GL_PLATFORM_WINDOWS
			char* value = nullptr;
			size_t length = 0;
			const bool enabled = _dupenv_s(&value, &length,
				"GLIMMER_CLIMATE_VALIDATE") == 0
				&& value != nullptr && std::string(value) == "1";
			std::free(value);
			return enabled;
#else
			const char* value = std::getenv("GLIMMER_CLIMATE_VALIDATE");
			return value && std::string(value) == "1";
#endif
		}

		Ref<Texture2D> ResolveLayerTexture(AssetHandle handle,
			TextureColorSpace colorSpace, TextureSemantic semantic)
		{
			const AssetMetadata metadata = AssetManager::GetMetadata(handle);
			if (!metadata.IsValid() || metadata.Type != AssetType::Texture2D
				|| metadata.ColorSpace != colorSpace || metadata.Semantic != semantic)
				return nullptr;
			return AssetManager::GetTexture2D(handle);
		}
	}

	void TerrainRenderer::Init()
	{
		if (!s_Data.Timer)
			s_Data.Timer = GPUTimer::Create();
	}

	void TerrainRenderer::Shutdown()
	{
		if (s_Data.PassActive && s_Data.Timer)
			s_Data.Timer->End();
		s_Data.PassActive = false;
		s_Data.Timer.reset();
		s_Data.Stats = {};
		s_Data.HasGpuTiming = false;
		s_Data.GpuTimingSample = 0;
	}

	void TerrainRenderer::BeginScene(float deltaSeconds)
	{
		if (!s_Data.HydrologyEnvironmentValidationChecked)
		{
			s_Data.HydrologyValidationRequested = ShouldValidateHydrology();
			s_Data.HydrologyEnvironmentValidationChecked = true;
		}
		if (!s_Data.ClimateEnvironmentValidationChecked)
		{
			s_Data.ClimateValidationRequested = ShouldValidateClimate();
			s_Data.ClimateEnvironmentValidationChecked = true;
		}
		s_Data.DeltaSeconds = std::max(deltaSeconds, 0.0f);
		++s_Data.FrameSerial;
		s_Data.HydrologyFrameActive = true;
		if (!s_Data.Timer)
			s_Data.Timer = GPUTimer::Create();
		float elapsedMilliseconds = 0.0f;
		if (s_Data.Timer
			&& s_Data.Timer->TryGetElapsedMilliseconds(elapsedMilliseconds))
		{
			s_Data.LastGpuMilliseconds = elapsedMilliseconds;
			s_Data.HasGpuTiming = true;
			++s_Data.GpuTimingSample;
		}
		s_Data.Stats = {};
		s_Data.Stats.GpuMilliseconds = s_Data.LastGpuMilliseconds;
		s_Data.Stats.GpuTimingAvailable = s_Data.HasGpuTiming;
		s_Data.Stats.GpuTimingSample = s_Data.GpuTimingSample;
		s_Data.Stats.Mode = s_Data.Sampling;
		s_Data.Stats.DetailDistance = s_Data.DetailDistance;
		if (s_Data.Timer)
		{
			s_Data.Timer->Begin();
			s_Data.PassActive = true;
		}
	}

	void TerrainRenderer::EndScene()
	{
		s_Data.HydrologyFrameActive = false;
		s_Data.HydrologyReadbackRequested = false;
		s_Data.ClimateReadbackRequested = false;
		if (!s_Data.PassActive || !s_Data.Timer)
			return;
		s_Data.Timer->End();
		s_Data.PassActive = false;
		float elapsedMilliseconds = 0.0f;
		if (s_Data.Timer->TryGetElapsedMilliseconds(elapsedMilliseconds))
		{
			s_Data.LastGpuMilliseconds = elapsedMilliseconds;
			s_Data.HasGpuTiming = true;
			++s_Data.GpuTimingSample;
			s_Data.Stats.GpuMilliseconds = elapsedMilliseconds;
			s_Data.Stats.GpuTimingAvailable = true;
			s_Data.Stats.GpuTimingSample = s_Data.GpuTimingSample;
		}
	}

	bool TerrainRenderer::Prepare(TerrainComponent& component)
	{
		auto& specification = component.Specification;
		if (!component.Runtime)
			component.Runtime = CreateRef<TerrainRuntime>();
		auto& runtime = *component.Runtime;

		const uint32_t sharedMeshResolution =
			TerrainChunkLayout::CalculateSharedMeshResolution(
				specification.MeshResolution);
		if (!runtime.Mesh
			|| runtime.LoadedMeshResolution != sharedMeshResolution)
		{
			const auto resolutions =
				TerrainChunkLayout::CalculateLODResolutions(sharedMeshResolution);
			for (size_t level = 0; level < runtime.LODMeshes.size(); ++level)
				runtime.LODMeshes[level] = CreateRef<TerrainMesh>(resolutions[level]);
			runtime.Mesh = runtime.LODMeshes[0];
			runtime.LoadedMeshResolution = sharedMeshResolution;
		}

		if (specification.Procedural)
		{
			const auto generationPath = AssetManager::GetFileSystemPath(specification.GenerationShaderHandle);
			if (generationPath.empty())
				return false;
			const auto erosionPath = ResolveComputePath(
				specification.ErosionShaderHandle, generationPath,
				"ThermalErosion.comp");
			const auto derivationPath = ResolveComputePath(
				specification.DerivationShaderHandle, generationPath,
				"DeriveTerrainMaps.comp");
			if (!runtime.Generator
				|| runtime.LoadedGenerationShaderHandle != specification.GenerationShaderHandle
				|| runtime.LoadedErosionShaderHandle != specification.ErosionShaderHandle
				|| runtime.LoadedDerivationShaderHandle != specification.DerivationShaderHandle
				|| runtime.LoadedHeightMapResolution != specification.HeightMapResolution)
			{
				runtime.Generator = CreateScope<TerrainGenerator>(
					CreateGridSpecification(std::max(specification.HeightMapResolution, 1u)),
					generationPath.string(), erosionPath.string(), derivationPath.string());
				runtime.LoadedGenerationShaderHandle = specification.GenerationShaderHandle;
				runtime.LoadedErosionShaderHandle = specification.ErosionShaderHandle;
				runtime.LoadedDerivationShaderHandle = specification.DerivationShaderHandle;
				runtime.LoadedHeightMapResolution = specification.HeightMapResolution;
				runtime.Dirty = true;
			}
			if (runtime.Generator->ReloadShadersIfChanged())
				runtime.Dirty = true;
			if (runtime.Dirty)
			{
				runtime.Generator->Generate(specification,
					static_cast<float>(
						std::max(specification.MeshResolution, 1u)));
				runtime.LastGenerationDispatchCount =
					runtime.Generator->GetLastDispatchCount();
				++runtime.GenerationVersion;
				runtime.Dirty = false;
			}
			runtime.HeightMap = runtime.Generator->GetHeightMap();
			runtime.NormalSlopeMap = runtime.Generator->GetNormalSlopeMap();
			runtime.AnalysisMap = runtime.Generator->GetAnalysisMap();
			runtime.MaterialWeightMap = runtime.Generator->GetMaterialWeightMap();

			if (!runtime.ValidationComplete && ShouldValidateTerrain())
			{
				const TerrainValidationResult first =
					runtime.Generator->ValidateOutputs();
				runtime.Generator->Generate(specification,
					static_cast<float>(
						std::max(specification.MeshResolution, 1u)));
				const TerrainValidationResult second =
					runtime.Generator->ValidateOutputs();
				runtime.Generator->DeriveMapsFromHeight(
					runtime.Generator->GetHeightMap(), specification.HeightScale,
					static_cast<float>(
						std::max(specification.MeshResolution, 1u)));
				const TerrainValidationResult runtimeDerived =
					runtime.Generator->ValidateOutputs();
				runtime.HeightMap = runtime.Generator->GetHeightMap();
				runtime.NormalSlopeMap = runtime.Generator->GetNormalSlopeMap();
				runtime.AnalysisMap = runtime.Generator->GetAnalysisMap();
				runtime.MaterialWeightMap = runtime.Generator->GetMaterialWeightMap();
				runtime.LastGenerationDispatchCount =
					runtime.Generator->GetLastDispatchCount();
				++runtime.GenerationVersion;
				runtime.ValidationComplete = runtimeDerived.Valid;
				if (second.Hash != runtimeDerived.Hash)
					runtime.ValidationComplete = false;
				if (first.Valid && second.Valid && first.Hash == second.Hash)
				{
					GL_CORE_INFO(
						"Terrain GPU validation PASS: deterministic hash={0}, dispatches={1}",
						first.Hash, runtime.LastGenerationDispatchCount);
				}
				else
				{
					GL_CORE_ERROR(
						"Terrain GPU validation FAIL: first={0}, second={1}, hashMatch={2}",
						first.Message, second.Message, first.Hash == second.Hash);
				}
			}
		}
		else if (runtime.LoadedHeightMapHandle != specification.HeightMapHandle || !runtime.HeightMap)
		{
			runtime.HeightMap = AssetManager::GetTexture2D(specification.HeightMapHandle);
			runtime.LoadedHeightMapHandle = specification.HeightMapHandle;
			runtime.NormalSlopeMap.reset();
			runtime.AnalysisMap.reset();
			runtime.MaterialWeightMap.reset();
		}

		if (!runtime.HeightMap)
			return false;

		if (specification.Procedural
			&& runtime.HeightMap->GetFormat() == TextureFormat::R32F)
		{
			const auto generationPath =
				AssetManager::GetFileSystemPath(specification.GenerationShaderHandle);
			const auto fluxPath = generationPath.parent_path() / "HydrologyFlux.comp";
			const auto updatePath = generationPath.parent_path() / "HydrologyUpdate.comp";
			const auto sedimentPath =
				generationPath.parent_path() / "SedimentTransport.comp";
			const auto capacityPath =
				generationPath.parent_path() / "SedimentCapacity.comp";
			const auto erosionPath =
				generationPath.parent_path() / "ErosionDeposition.comp";
			if (s_Data.HydrologyValidationRequested)
			{
				s_Data.HydrologyValidation =
					TerrainHydrologyGPU::ValidateContract(
						fluxPath, updatePath, sedimentPath,
						capacityPath, erosionPath);
				s_Data.HydrologyValidationRequested = false;
				if (s_Data.HydrologyValidation.Passed)
					GL_CORE_INFO("GPU hydrology contract validation {0}",
						s_Data.HydrologyValidation.Message);
				else
					GL_CORE_ERROR("GPU hydrology contract validation {0}",
						s_Data.HydrologyValidation.Message);
			}
			if (!runtime.GPUHydrology
				|| runtime.HydrologyGenerationVersion != runtime.GenerationVersion)
			{
				runtime.GPUHydrology = CreateScope<TerrainHydrologyGPU>(
					runtime.HeightMap->GetWidth(), runtime.HeightMap->GetHeight(),
					fluxPath, updatePath, sedimentPath,
					capacityPath, erosionPath);
				runtime.GPUHydrology->SetInitialHeightMap(
					runtime.HeightMap, specification.HeightScale,
					static_cast<float>(
						std::max(specification.MeshResolution, 1u)));
				runtime.HydrologyGenerationVersion = runtime.GenerationVersion;
			}
			auto& hydrology = *runtime.GPUHydrology;
			hydrology.ReloadShadersIfChanged();
			hydrology.GetSettings().RainfallRate = s_Data.HydrologyRainfall;
			hydrology.GetSettings().ErosionRate = s_Data.HydrologyErosionRate;
			hydrology.GetSettings().DepositionRate =
				s_Data.HydrologyDepositionRate;
			hydrology.GetSettings().TerrainDensity =
				s_Data.HydrologyTerrainDensity;
			hydrology.GetSettings().MaximumErosionDepth =
				s_Data.HydrologyMaximumErosionDepth;
			hydrology.GetSettings().MaximumHeightChangePerStep =
				s_Data.HydrologyMaximumHeightChange;
			if (hydrology.GetSettings().SedimentCapacityScale
				!= s_Data.HydrologySedimentCapacityScale)
			{
				hydrology.SetSedimentCapacityScale(
					s_Data.HydrologySedimentCapacityScale);
			}
			runtime.HeightMap = hydrology.GetHeightTexture();

			const auto climateSourcePath =
				generationPath.parent_path() / "ClimateSource.comp";
			const auto climateAdvectionPath =
				generationPath.parent_path() / "ClimateAdvection.comp";
			const auto climateResponsePath =
				generationPath.parent_path() / "ClimateResponse.comp";
			const auto climateWaterSourcePath =
				generationPath.parent_path() / "ClimateWaterSource.comp";
			if (s_Data.ClimateValidationRequested)
			{
				s_Data.ClimateValidation =
					TerrainClimateGPU::ValidateContract(
						climateSourcePath, climateAdvectionPath,
						climateResponsePath, climateWaterSourcePath);
				s_Data.ClimateValidationRequested = false;
				if (s_Data.ClimateValidation.Passed)
					GL_CORE_INFO("GPU climate contract validation {0}",
						s_Data.ClimateValidation.Message);
				else
					GL_CORE_ERROR("GPU climate contract validation {0}",
						s_Data.ClimateValidation.Message);
			}
			if (!runtime.GPUClimate
				|| runtime.ClimateGenerationVersion != runtime.GenerationVersion)
			{
				runtime.GPUClimate = CreateScope<TerrainClimateGPU>(
					runtime.HeightMap->GetWidth(), runtime.HeightMap->GetHeight(),
					climateSourcePath, climateAdvectionPath, climateResponsePath,
					climateWaterSourcePath);
				runtime.ClimateGenerationVersion = runtime.GenerationVersion;
				runtime.GPUEnvironment.reset();
			}
			auto& climate = *runtime.GPUClimate;
			climate.ReloadShadersIfChanged();
			climate.GetSettings().WindVelocity = s_Data.ClimateWindVelocity;
			climate.GetSettings().InitialAtmosphericMoisture =
				s_Data.ClimateInitialMoisture;
			climate.GetSettings().TemperatureLapseRate =
				s_Data.ClimateTemperatureLapseRate;
			const float worldSize = static_cast<float>(
				std::max(specification.MeshResolution, 1u));
			if (!runtime.GPUEnvironment)
			{
				runtime.GPUEnvironment = CreateScope<TerrainEnvironmentGPU>(
					runtime.HeightMap->GetWidth(), runtime.HeightMap->GetHeight());
				runtime.GPUEnvironment->Reset(climate, hydrology, worldSize);
			}
			if (s_Data.HydrologyFrameActive
				&& runtime.ClimateFrameSerial != s_Data.FrameSerial)
			{
				auto& environment = *runtime.GPUEnvironment;
				bool resetApplied = false;
				bool environmentStepped = false;
				if (runtime.ClimateResetRequest != s_Data.ClimateResetRequest
					|| runtime.HydrologyResetRequest != s_Data.HydrologyResetRequest)
				{
					environment.Reset(climate, hydrology, worldSize);
					resetApplied = true;
					runtime.ClimateResetRequest = s_Data.ClimateResetRequest;
					runtime.HydrologyResetRequest = s_Data.HydrologyResetRequest;
				}
				if (runtime.HydrologySedimentSeedRequest
					!= s_Data.HydrologySedimentSeedRequest)
				{
					hydrology.SetUniformSedimentDensity(
						s_Data.HydrologySedimentSeedDensity, worldSize);
					runtime.HydrologySedimentSeedRequest =
						s_Data.HydrologySedimentSeedRequest;
				}
				if (runtime.ClimateSingleStepRequest != s_Data.ClimateSingleStepRequest
					|| runtime.HydrologySingleStepRequest
						!= s_Data.HydrologySingleStepRequest)
				{
					environment.SingleStep(climate, hydrology,
						specification.HeightScale, worldSize);
					environmentStepped = true;
					runtime.ClimateSingleStepRequest =
						s_Data.ClimateSingleStepRequest;
					runtime.HydrologySingleStepRequest =
						s_Data.HydrologySingleStepRequest;
				}
				if (s_Data.ClimatePlaying || s_Data.HydrologyPlaying)
				{
					environmentStepped = environment.Advance(
						s_Data.DeltaSeconds, climate, hydrology,
						specification.HeightScale, worldSize) != 0
						|| environmentStepped;
				}
				runtime.HeightMap = hydrology.GetHeightTexture();
				bool refreshDerivedMaps = resetApplied;
				if (environmentStepped)
				{
					const auto& settings = hydrology.GetSettings();
					refreshDerivedMaps = settings.MaximumHeightChangePerStep > 0.0f
						&& (settings.DepositionRate > 0.0f
							|| (settings.ErosionRate > 0.0f
								&& settings.MaximumErosionDepth > 0.0f));
				}
				if (refreshDerivedMaps)
				{
					runtime.Generator->DeriveMapsFromHeight(runtime.HeightMap,
						specification.HeightScale, worldSize);
					runtime.NormalSlopeMap = runtime.Generator->GetNormalSlopeMap();
					runtime.AnalysisMap = runtime.Generator->GetAnalysisMap();
					runtime.MaterialWeightMap =
						runtime.Generator->GetMaterialWeightMap();
					++s_Data.Stats.RuntimeDerivedMapRefreshes;
				}
				if (s_Data.ClimateReadbackRequested
					|| s_Data.HydrologyReadbackRequested)
				{
					environment.ReadbackStatistics(climate, hydrology,
						worldSize, specification.HeightScale);
				}
				s_Data.ClimateStats = climate.GetStatistics();
				s_Data.HydrologyStats = hydrology.GetStatistics();
				s_Data.EnvironmentStats = environment.GetStatistics();
				runtime.ClimateFrameSerial = s_Data.FrameSerial;
				runtime.HydrologyFrameSerial = s_Data.FrameSerial;
			}
		}
		return true;
	}

	void TerrainRenderer::Draw(TerrainComponent& component, const glm::mat4& transform,
		const glm::mat4& viewProjection, const glm::vec3& cameraPosition, int entityID)
	{
		if (!Prepare(component))
			return;
		auto& specification = component.Specification;
		auto& runtime = *component.Runtime;
		const Ref<Shader> shader = AssetManager::GetShader(specification.RenderShaderHandle);
		if (!shader)
			return;

		shader->ReloadIfChanged();
		shader->Bind();
		shader->UploadUniformMat4("u_ViewProjection", viewProjection);
		shader->UploadUniformMat4("u_Transform", transform);
		shader->UploadUniformFloat3("u_CameraPos", cameraPosition);
		shader->UploadUniformInt("u_EntityID", entityID);
		shader->UploadUniformFloat("u_MaxHeight", specification.HeightScale);
		shader->UploadUniformFloat("u_UVScale", 1.0f);
		shader->UploadUniformFloat2("u_TexelSize", {
			1.0f / static_cast<float>(runtime.HeightMap->GetWidth()),
			1.0f / static_cast<float>(runtime.HeightMap->GetHeight()) });
		const uint32_t sampleCount = runtime.HeightMap->GetWidth() > 1
			? runtime.HeightMap->GetWidth() - 1 : 1;
		const float terrainWorldSize = static_cast<float>(
			std::max(specification.MeshResolution, 1u));
		const float sampleSpacing = terrainWorldSize
			/ static_cast<float>(sampleCount);
		shader->UploadUniformFloat("u_SampleSpacing", sampleSpacing);
		shader->UploadUniformInt("u_TerrainSamplingMode",
			static_cast<int>(s_Data.Sampling));
		shader->UploadUniformFloat("u_TerrainDetailDistance",
			s_Data.DetailDistance);
		const float skirtDepth = std::max(
			2.0f, std::abs(specification.HeightScale) * 0.08f);
		shader->UploadUniformFloat("u_SkirtDepth", skirtDepth);
		shader->UploadUniformInt("u_TerrainLODVisualization",
			s_Data.VisualizeLODs ? 1 : 0);
		const bool hasHydrology = runtime.GPUHydrology != nullptr;
		const bool hasClimate = runtime.GPUClimate != nullptr;
		shader->UploadUniformInt("u_HasHydrology", hasHydrology ? 1 : 0);
		shader->UploadUniformInt("u_HydrologyVisualization",
			hasHydrology
				? static_cast<int>(s_Data.HydrologyVisualization) : 0);
		if (hasHydrology)
		{
			runtime.GPUHydrology->GetWaterTexture()->Bind(23);
			runtime.GPUHydrology->GetVelocityTexture()->Bind(24);
			runtime.GPUHydrology->GetSedimentTexture()->Bind(25);
			runtime.GPUHydrology->GetSedimentCapacityTexture()->Bind(26);
			runtime.GPUHydrology->GetSedimentSaturationTexture()->Bind(27);
			shader->UploadUniformInt("u_WaterDepthMap", 23);
			shader->UploadUniformInt("u_WaterVelocityMap", 24);
			shader->UploadUniformInt("u_SedimentMap", 25);
			shader->UploadUniformInt("u_SedimentCapacityMap", 26);
			shader->UploadUniformInt("u_SedimentSaturationMap", 27);
		}
		shader->UploadUniformInt("u_HasClimate", hasClimate ? 1 : 0);
		shader->UploadUniformInt("u_ClimateVisualization",
			static_cast<int>(s_Data.ClimateVisualization));
		if (hasClimate)
		{
			runtime.GPUClimate->GetTemperatureTexture()->Bind(28);
			runtime.GPUClimate->GetAtmosphericMoistureTexture()->Bind(29);
			runtime.GPUClimate->GetRainfallTexture()->Bind(30);
			runtime.GPUClimate->GetVegetationPotentialTexture()->Bind(31);
			shader->UploadUniformInt("u_TemperatureMap", 28);
			shader->UploadUniformInt("u_AtmosphericMoistureMap", 29);
			shader->UploadUniformInt("u_RainfallMap", 30);
			shader->UploadUniformInt("u_VegetationPotentialMap", 31);
		}
		runtime.HeightMap->Bind(0);
		shader->UploadUniformInt("u_HeightMap", 0);
		ShadowRenderer::BindForLighting(shader, 16);
		EnvironmentLighting::BindForLighting(shader, 20, 21, 22);
		const bool hasDerivedMaps = runtime.NormalSlopeMap
			&& runtime.AnalysisMap && runtime.MaterialWeightMap;
		shader->UploadUniformInt("u_HasDerivedMaps", hasDerivedMaps ? 1 : 0);
		if (hasDerivedMaps)
		{
			runtime.NormalSlopeMap->Bind(1);
			runtime.AnalysisMap->Bind(2);
			runtime.MaterialWeightMap->Bind(3);
			shader->UploadUniformInt("u_NormalSlopeMap", 1);
			shader->UploadUniformInt("u_TerrainAnalysisMap", 2);
			shader->UploadUniformInt("u_MaterialWeightMap", 3);
		}

		TerrainMaterialProperties materialProperties =
			TerrainMaterial::CreateDefaultProperties();
		if (const Ref<TerrainMaterial> terrainMaterial =
			AssetManager::GetTerrainMaterial(specification.TerrainMaterialHandle))
			materialProperties = terrainMaterial->GetProperties();
		shader->UploadUniformFloat("u_TriplanarSharpness",
			materialProperties.TriplanarSharpness);
		shader->UploadUniformFloat("u_WeightContrast",
			materialProperties.WeightContrast);
		shader->UploadUniformFloat("u_HeightInfluence",
			materialProperties.HeightInfluence);
		shader->UploadUniformFloat("u_SlopeInfluence",
			materialProperties.SlopeInfluence);
		shader->UploadUniformFloat("u_CurvatureInfluence",
			materialProperties.CurvatureInfluence);
		shader->UploadUniformFloat("u_MoistureInfluence",
			materialProperties.MoistureInfluence);
		for (size_t index = 0; index < materialProperties.Layers.size(); ++index)
		{
			const auto& layer = materialProperties.Layers[index];
			const std::string prefix = "u_Layers[" + std::to_string(index) + "].";
			shader->UploadUniformFloat3(prefix + "BaseColor", layer.BaseColor);
			shader->UploadUniformFloat(prefix + "Tiling", layer.Tiling);
			shader->UploadUniformFloat(prefix + "Metallic", layer.Metallic);
			shader->UploadUniformFloat(prefix + "Roughness", layer.Roughness);
			shader->UploadUniformFloat(prefix + "NormalScale", layer.NormalScale);
			shader->UploadUniformFloat(prefix + "AOStrength", layer.AOStrength);

			const uint32_t albedoSlot = 4 + static_cast<uint32_t>(index) * 3;
			const uint32_t normalSlot = albedoSlot + 1;
			const uint32_t aoSlot = albedoSlot + 2;
			const Ref<Texture2D> albedo = ResolveLayerTexture(layer.AlbedoTexture,
				TextureColorSpace::SRGB, TextureSemantic::Color);
			const Ref<Texture2D> normal = ResolveLayerTexture(layer.NormalTexture,
				TextureColorSpace::Linear, TextureSemantic::Normal);
			const Ref<Texture2D> ao = ResolveLayerTexture(layer.AOTexture,
				TextureColorSpace::Linear, TextureSemantic::Data);
			shader->UploadUniformInt("u_AlbedoTextures[" + std::to_string(index) + "]",
				static_cast<int>(albedoSlot));
			shader->UploadUniformInt("u_NormalTextures[" + std::to_string(index) + "]",
				static_cast<int>(normalSlot));
			shader->UploadUniformInt("u_AOTextures[" + std::to_string(index) + "]",
				static_cast<int>(aoSlot));
			shader->UploadUniformInt(prefix + "HasAlbedo", albedo ? 1 : 0);
			shader->UploadUniformInt(prefix + "HasNormal", normal ? 1 : 0);
			shader->UploadUniformInt(prefix + "HasAO", ao ? 1 : 0);
			if (albedo) albedo->Bind(albedoSlot);
			if (normal) normal->Bind(normalSlot);
			if (ao) ao->Bind(aoSlot);
			s_Data.Stats.BoundMaterialTextures += static_cast<uint32_t>(
				static_cast<bool>(albedo) + static_cast<bool>(normal)
				+ static_cast<bool>(ao));
		}
		const auto chunks = TerrainChunkLayout::Build(
			terrainWorldSize, runtime.Mesh->GetGridSize());
		std::array<uint32_t, TerrainChunkLayout::ChunkCount> lodLevels{};
		for (size_t index = 0; index < chunks.size(); ++index)
		{
			const glm::vec4 worldCenter = transform * glm::vec4(
				chunks[index].LocalOffset.x,
				(specification.HeightScale * 0.5f),
				chunks[index].LocalOffset.y, 1.0f);
			const float distance = glm::distance(
				cameraPosition, glm::vec3(worldCenter));
			lodLevels[index] = runtime.HasChunkLODHistory
				? TerrainChunkLayout::SelectLODLevelWithHysteresis(
					distance, s_Data.LODMiddleDistance, s_Data.LODFarDistance,
					runtime.ChunkLODLevels[index], s_Data.LODHysteresis)
				: TerrainChunkLayout::SelectLODLevel(distance,
					s_Data.LODMiddleDistance, s_Data.LODFarDistance);
		}
		lodLevels = TerrainChunkLayout::StabilizeNeighborLODs(lodLevels);
		runtime.ChunkLODLevels = lodLevels;
		runtime.HasChunkLODHistory = true;
		const float minimumHeight = std::min(0.0f, specification.HeightScale);
		const float maximumHeight = std::max(0.0f, specification.HeightScale);
		s_Data.Stats.SharedMeshes = static_cast<uint32_t>(runtime.LODMeshes.size());
		for (size_t chunkIndex = 0; chunkIndex < chunks.size(); ++chunkIndex)
		{
			const TerrainChunkRegion& chunk = chunks[chunkIndex];
			++s_Data.Stats.CandidateChunks;
			const float halfSize = chunk.WorldSize * 0.5f;
			const glm::vec3 boundsMin(
				chunk.LocalOffset.x - halfSize,
				minimumHeight,
				chunk.LocalOffset.y - halfSize);
			const glm::vec3 boundsMax(
				chunk.LocalOffset.x + halfSize,
				maximumHeight,
				chunk.LocalOffset.y + halfSize);
			if (!IntersectsCameraFrustum(
				boundsMin, boundsMax, transform, viewProjection))
			{
				++s_Data.Stats.CulledChunks;
				continue;
			}
			const uint32_t lodLevel = lodLevels[chunkIndex];
			const Ref<TerrainMesh>& lodMesh = runtime.LODMeshes[lodLevel];
			if (!lodMesh)
				continue;
			shader->UploadUniformFloat2(
				"u_ChunkUVOffset", chunk.UVOffset);
			shader->UploadUniformFloat2(
				"u_ChunkUVScale", chunk.UVScale);
			shader->UploadUniformFloat2(
				"u_ChunkLocalOffset", chunk.LocalOffset);
			shader->UploadUniformFloat(
				"u_ChunkLocalScale", chunk.WorldSize
					/ static_cast<float>(lodMesh->GetGridSize()));
			shader->UploadUniformInt("u_TerrainLODLevel",
				static_cast<int>(lodLevel));
			RenderCommand::DrawIndexed(
				lodMesh->GetVertexArray(), lodMesh->GetIndexCount());
			++s_Data.Stats.SubmittedChunks;
			++s_Data.Stats.DrawCalls;
			switch (lodLevel)
			{
			case 0: ++s_Data.Stats.LOD0Chunks; break;
			case 1: ++s_Data.Stats.LOD1Chunks; break;
			default: ++s_Data.Stats.LOD2Chunks; break;
			}
			s_Data.Stats.SubmittedTriangles += lodMesh->GetIndexCount() / 3u;
		}
	}

	void TerrainRenderer::Invalidate(TerrainComponent& component)
	{
		if (component.Runtime)
		{
			component.Runtime->Dirty = true;
			component.Runtime->ValidationComplete = false;
		}
	}

	void TerrainRenderer::SetSamplingMode(SamplingMode mode)
	{
		s_Data.Sampling = mode;
	}

	TerrainRenderer::SamplingMode TerrainRenderer::GetSamplingMode()
	{
		return s_Data.Sampling;
	}

	void TerrainRenderer::SetDetailDistance(float distance)
	{
		s_Data.DetailDistance = std::clamp(distance, 1.0f, 10000.0f);
	}

	float TerrainRenderer::GetDetailDistance()
	{
		return s_Data.DetailDistance;
	}

	void TerrainRenderer::SetLODDistances(float middleDistance, float farDistance)
	{
		s_Data.LODMiddleDistance = std::clamp(middleDistance, 1.0f, 10000.0f);
		s_Data.LODFarDistance = std::clamp(
			farDistance, s_Data.LODMiddleDistance + 1.0f, 20000.0f);
	}

	glm::vec2 TerrainRenderer::GetLODDistances()
	{
		return { s_Data.LODMiddleDistance, s_Data.LODFarDistance };
	}

	void TerrainRenderer::SetLODVisualizationEnabled(bool enabled)
	{
		s_Data.VisualizeLODs = enabled;
	}

	bool TerrainRenderer::IsLODVisualizationEnabled()
	{
		return s_Data.VisualizeLODs;
	}

	void TerrainRenderer::SetHydrologyPlaying(bool playing)
	{
		s_Data.HydrologyPlaying = playing;
	}

	bool TerrainRenderer::IsHydrologyPlaying()
	{
		return s_Data.HydrologyPlaying;
	}

	void TerrainRenderer::RequestHydrologySingleStep()
	{
		if (!s_Data.HydrologyPlaying)
			++s_Data.HydrologySingleStepRequest;
	}

	void TerrainRenderer::RequestHydrologyReset()
	{
		++s_Data.HydrologyResetRequest;
		s_Data.HydrologyStats = {};
	}

	void TerrainRenderer::SetHydrologyRainfall(float rainfallRate)
	{
		s_Data.HydrologyRainfall = std::clamp(rainfallRate, 0.0f, 5.0f);
	}

	float TerrainRenderer::GetHydrologyRainfall()
	{
		return s_Data.HydrologyRainfall;
	}

	void TerrainRenderer::SetHydrologySedimentCapacityScale(float capacityScale)
	{
		s_Data.HydrologySedimentCapacityScale = std::clamp(
			capacityScale, 0.0f, 1000.0f);
	}

	float TerrainRenderer::GetHydrologySedimentCapacityScale()
	{
		return s_Data.HydrologySedimentCapacityScale;
	}

	void TerrainRenderer::SetHydrologyErosionRate(float erosionRate)
	{
		s_Data.HydrologyErosionRate = std::clamp(
			erosionRate, 0.0f, 1000.0f);
	}

	float TerrainRenderer::GetHydrologyErosionRate()
	{
		return s_Data.HydrologyErosionRate;
	}

	void TerrainRenderer::SetHydrologyDepositionRate(float depositionRate)
	{
		s_Data.HydrologyDepositionRate = std::clamp(
			depositionRate, 0.0f, 1000.0f);
	}

	float TerrainRenderer::GetHydrologyDepositionRate()
	{
		return s_Data.HydrologyDepositionRate;
	}

	void TerrainRenderer::SetHydrologyTerrainDensity(float terrainDensity)
	{
		s_Data.HydrologyTerrainDensity = std::clamp(
			terrainDensity, 1.0e-6f, 1000000.0f);
	}

	float TerrainRenderer::GetHydrologyTerrainDensity()
	{
		return s_Data.HydrologyTerrainDensity;
	}

	void TerrainRenderer::SetHydrologyMaximumErosionDepth(float depth)
	{
		s_Data.HydrologyMaximumErosionDepth = std::clamp(
			depth, 0.0f, 10000.0f);
	}

	float TerrainRenderer::GetHydrologyMaximumErosionDepth()
	{
		return s_Data.HydrologyMaximumErosionDepth;
	}

	void TerrainRenderer::SetHydrologyMaximumHeightChange(float heightChange)
	{
		s_Data.HydrologyMaximumHeightChange = std::clamp(
			heightChange, 0.0f, 1000.0f);
	}

	float TerrainRenderer::GetHydrologyMaximumHeightChange()
	{
		return s_Data.HydrologyMaximumHeightChange;
	}

	void TerrainRenderer::SetHydrologyVisualizationMode(
		HydrologyVisualizationMode mode)
	{
		s_Data.HydrologyVisualization = mode;
	}

	TerrainRenderer::HydrologyVisualizationMode
	TerrainRenderer::GetHydrologyVisualizationMode()
	{
		return s_Data.HydrologyVisualization;
	}

	void TerrainRenderer::SetHydrologySedimentSeedDensity(float sedimentDensity)
	{
		s_Data.HydrologySedimentSeedDensity = std::clamp(
			sedimentDensity, 0.0f, 1000.0f);
	}

	float TerrainRenderer::GetHydrologySedimentSeedDensity()
	{
		return s_Data.HydrologySedimentSeedDensity;
	}

	void TerrainRenderer::RequestHydrologySedimentSeed()
	{
		++s_Data.HydrologySedimentSeedRequest;
	}

	void TerrainRenderer::RequestHydrologyReadback()
	{
		s_Data.HydrologyReadbackRequested = true;
	}

	TerrainHydrologyGPUStatistics TerrainRenderer::GetHydrologyStatistics()
	{
		return s_Data.HydrologyStats;
	}

	void TerrainRenderer::RequestHydrologyContractValidation()
	{
		s_Data.HydrologyValidationRequested = true;
	}

	TerrainHydrologyGPUValidationResult
	TerrainRenderer::GetHydrologyValidationResult()
	{
		return s_Data.HydrologyValidation;
	}

	void TerrainRenderer::SetClimatePlaying(bool playing)
	{
		s_Data.ClimatePlaying = playing;
	}

	bool TerrainRenderer::IsClimatePlaying()
	{
		return s_Data.ClimatePlaying;
	}

	void TerrainRenderer::RequestClimateSingleStep()
	{
		if (!s_Data.ClimatePlaying)
			++s_Data.ClimateSingleStepRequest;
	}

	void TerrainRenderer::RequestClimateReset()
	{
		++s_Data.ClimateResetRequest;
		s_Data.ClimateStats = {};
	}

	void TerrainRenderer::SetClimateWindVelocity(const glm::vec2& velocity)
	{
		if (std::isfinite(velocity.x) && std::isfinite(velocity.y))
			s_Data.ClimateWindVelocity = velocity;
	}

	glm::vec2 TerrainRenderer::GetClimateWindVelocity()
	{
		return s_Data.ClimateWindVelocity;
	}

	void TerrainRenderer::SetClimateInitialMoisture(float moistureDepth)
	{
		s_Data.ClimateInitialMoisture = std::isfinite(moistureDepth)
			? std::clamp(moistureDepth, 0.0f, 10.0f) : 0.0f;
	}

	float TerrainRenderer::GetClimateInitialMoisture()
	{
		return s_Data.ClimateInitialMoisture;
	}

	void TerrainRenderer::SetClimateTemperatureLapseRate(float lapseRate)
	{
		s_Data.ClimateTemperatureLapseRate = std::isfinite(lapseRate)
			? std::clamp(lapseRate, 0.0f, 1.0f) : 0.0065f;
	}

	float TerrainRenderer::GetClimateTemperatureLapseRate()
	{
		return s_Data.ClimateTemperatureLapseRate;
	}

	void TerrainRenderer::SetClimateVisualizationMode(
		ClimateVisualizationMode mode)
	{
		s_Data.ClimateVisualization = mode;
	}

	TerrainRenderer::ClimateVisualizationMode
	TerrainRenderer::GetClimateVisualizationMode()
	{
		return s_Data.ClimateVisualization;
	}

	void TerrainRenderer::RequestClimateReadback()
	{
		s_Data.ClimateReadbackRequested = true;
	}

	TerrainClimateGPUStatistics TerrainRenderer::GetClimateStatistics()
	{
		return s_Data.ClimateStats;
	}

	void TerrainRenderer::RequestClimateContractValidation()
	{
		s_Data.ClimateValidationRequested = true;
	}

	TerrainClimateGPUValidationResult
	TerrainRenderer::GetClimateValidationResult()
	{
		return s_Data.ClimateValidation;
	}

	TerrainEnvironmentGPUStatistics TerrainRenderer::GetEnvironmentStatistics()
	{
		return s_Data.EnvironmentStats;
	}

	TerrainRenderer::Statistics TerrainRenderer::GetStatistics()
	{
		return s_Data.Stats;
	}

	bool TerrainRenderer::IntersectsCameraFrustum(
		const glm::vec3& boundsMin,
		const glm::vec3& boundsMax,
		const glm::mat4& transform,
		const glm::mat4& viewProjection)
	{
		return FrustumCulling::IntersectsClipFrustum(
			boundsMin, boundsMax, transform, viewProjection);
	}
}
