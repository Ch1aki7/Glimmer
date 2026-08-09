#include "glpch.h"
#include "TerrainRenderer.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Terrain/Terrain.h"

#include <cstdlib>

namespace gl {
	namespace {
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
	}

	void TerrainRenderer::Draw(TerrainComponent& component, const glm::mat4& transform,
		const glm::mat4& viewProjection, const glm::vec3& cameraPosition, int entityID)
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

		const Ref<Shader> shader = AssetManager::GetShader(specification.RenderShaderHandle);
		if (!shader)
			return;

		if (specification.Procedural)
		{
			const auto generationPath = AssetManager::GetFileSystemPath(specification.GenerationShaderHandle);
			if (generationPath.empty())
				return;
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
		runtime.HeightMap->Bind(0);
		shader->UploadUniformInt("u_HeightMap", 0);
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
		runtime.Mesh->Bind();
		RenderCommand::DrawIndexed(runtime.Mesh->GetVertexArray(), runtime.Mesh->GetIndexCount());
	}

	void TerrainRenderer::Invalidate(TerrainComponent& component)
	{
		if (component.Runtime)
		{
			component.Runtime->Dirty = true;
			component.Runtime->ValidationComplete = false;
		}
	}
}
