#include "glpch.h"
#include "TerrainRenderer.h"
#include "EnvironmentLighting.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/ShadowRenderer.h"
#include "Glimmer/Renderer/GPUTimer.h"
#include "Glimmer/Terrain/Terrain.h"
#include "Glimmer/Terrain/TerrainMaterial.h"

#include <cstdlib>

namespace gl {
	namespace {
		struct TerrainRendererData
		{
			Ref<GPUTimer> Timer;
			TerrainRenderer::Statistics Stats;
			TerrainRenderer::SamplingMode Sampling =
				TerrainRenderer::SamplingMode::AutomaticDistance;
			float DetailDistance = 80.0f;
			float LastGpuMilliseconds = 0.0f;
			uint64_t GpuTimingSample = 0;
			bool HasGpuTiming = false;
			bool PassActive = false;
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

	void TerrainRenderer::BeginScene()
	{
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

		if (!runtime.Mesh || runtime.LoadedMeshResolution != specification.MeshResolution)
		{
			runtime.Mesh = CreateRef<TerrainMesh>(std::max(specification.MeshResolution, 1u));
			runtime.LoadedMeshResolution = specification.MeshResolution;
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
					static_cast<float>(runtime.Mesh->GetGridSize()));
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
					static_cast<float>(runtime.Mesh->GetGridSize()));
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
		const float sampleSpacing = static_cast<float>(runtime.Mesh->GetGridSize())
			/ static_cast<float>(sampleCount);
		shader->UploadUniformFloat("u_SampleSpacing", sampleSpacing);
		shader->UploadUniformInt("u_TerrainSamplingMode",
			static_cast<int>(s_Data.Sampling));
		shader->UploadUniformFloat("u_TerrainDetailDistance",
			s_Data.DetailDistance);
		runtime.HeightMap->Bind(0);
		shader->UploadUniformInt("u_HeightMap", 0);
		ShadowRenderer::BindForLighting(shader, 16);
		EnvironmentLighting::BindForLighting(shader, 20);
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
		runtime.Mesh->Bind();
		RenderCommand::DrawIndexed(runtime.Mesh->GetVertexArray(), runtime.Mesh->GetIndexCount());
		++s_Data.Stats.DrawCalls;
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

	TerrainRenderer::Statistics TerrainRenderer::GetStatistics()
	{
		return s_Data.Stats;
	}
}
