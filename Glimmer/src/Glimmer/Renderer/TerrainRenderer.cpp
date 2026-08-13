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
			bool VisualizeHydrology = false;
			bool HydrologyReadbackRequested = false;
			float HydrologyRainfall = 0.02f;
			float DeltaSeconds = 0.0f;
			uint64_t HydrologyResetRequest = 0;
			uint64_t HydrologySingleStepRequest = 0;
			TerrainHydrologyGPUStatistics HydrologyStats;
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
				runtime.HeightMap = runtime.Generator->GetHeightMap();
				runtime.NormalSlopeMap = runtime.Generator->GetNormalSlopeMap();
				runtime.AnalysisMap = runtime.Generator->GetAnalysisMap();
				runtime.MaterialWeightMap = runtime.Generator->GetMaterialWeightMap();
				runtime.LastGenerationDispatchCount =
					runtime.Generator->GetLastDispatchCount();
				++runtime.GenerationVersion;
				runtime.ValidationComplete = true;
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
			if (!runtime.GPUHydrology
				|| runtime.HydrologyGenerationVersion != runtime.GenerationVersion)
			{
				runtime.GPUHydrology = CreateScope<TerrainHydrologyGPU>(
					runtime.HeightMap->GetWidth(), runtime.HeightMap->GetHeight(),
					fluxPath, updatePath);
				runtime.HydrologyGenerationVersion = runtime.GenerationVersion;
			}
			auto& hydrology = *runtime.GPUHydrology;
			hydrology.GetSettings().RainfallRate = s_Data.HydrologyRainfall;
			if (s_Data.HydrologyFrameActive
				&& runtime.HydrologyFrameSerial != s_Data.FrameSerial)
			{
				if (runtime.HydrologyResetRequest != s_Data.HydrologyResetRequest)
				{
					hydrology.Reset();
					runtime.HydrologyResetRequest = s_Data.HydrologyResetRequest;
				}
				const float worldSize = static_cast<float>(
					std::max(specification.MeshResolution, 1u));
				if (runtime.HydrologySingleStepRequest
					!= s_Data.HydrologySingleStepRequest)
				{
					hydrology.SingleStep(runtime.HeightMap,
						specification.HeightScale, worldSize);
					runtime.HydrologySingleStepRequest =
						s_Data.HydrologySingleStepRequest;
				}
				if (s_Data.HydrologyPlaying)
					hydrology.Advance(s_Data.DeltaSeconds, runtime.HeightMap,
						specification.HeightScale, worldSize);
				if (s_Data.HydrologyReadbackRequested)
					hydrology.ReadbackStatistics(worldSize);
				s_Data.HydrologyStats = hydrology.GetStatistics();
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
		shader->UploadUniformInt("u_HasHydrology", hasHydrology ? 1 : 0);
		shader->UploadUniformInt("u_HydrologyVisualization",
			s_Data.VisualizeHydrology && hasHydrology ? 1 : 0);
		if (hasHydrology)
		{
			runtime.GPUHydrology->GetWaterTexture()->Bind(23);
			runtime.GPUHydrology->GetVelocityTexture()->Bind(24);
			shader->UploadUniformInt("u_WaterDepthMap", 23);
			shader->UploadUniformInt("u_WaterVelocityMap", 24);
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

	void TerrainRenderer::SetHydrologyVisualizationEnabled(bool enabled)
	{
		s_Data.VisualizeHydrology = enabled;
	}

	bool TerrainRenderer::IsHydrologyVisualizationEnabled()
	{
		return s_Data.VisualizeHydrology;
	}

	void TerrainRenderer::RequestHydrologyReadback()
	{
		s_Data.HydrologyReadbackRequested = true;
	}

	TerrainHydrologyGPUStatistics TerrainRenderer::GetHydrologyStatistics()
	{
		return s_Data.HydrologyStats;
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
