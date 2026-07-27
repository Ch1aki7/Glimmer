#include "glpch.h"
#include "TerrainRenderer.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Terrain/Terrain.h"

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
			if (!runtime.Generator
				|| runtime.LoadedGenerationShaderHandle != specification.GenerationShaderHandle
				|| runtime.LoadedHeightMapResolution != specification.HeightMapResolution)
			{
				runtime.Generator = CreateScope<TerrainGenerator>(
					CreateGridSpecification(std::max(specification.HeightMapResolution, 1u)),
					generationPath.string());
				runtime.LoadedGenerationShaderHandle = specification.GenerationShaderHandle;
				runtime.LoadedHeightMapResolution = specification.HeightMapResolution;
				runtime.Dirty = true;
			}
			const ShaderReloadResult reload = runtime.Generator->ReloadShaderIfChanged();
			if (reload.Attempted && reload.Success)
				runtime.Dirty = true;
			if (runtime.Dirty)
			{
				runtime.Generator->Generate(specification.Noise);
				runtime.Dirty = false;
			}
			runtime.HeightMap = runtime.Generator->GetHeightMap();
		}
		else if (runtime.LoadedHeightMapHandle != specification.HeightMapHandle || !runtime.HeightMap)
		{
			runtime.HeightMap = AssetManager::GetTexture2D(specification.HeightMapHandle);
			runtime.LoadedHeightMapHandle = specification.HeightMapHandle;
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
		runtime.Mesh->Bind();
		RenderCommand::DrawIndexed(runtime.Mesh->GetVertexArray(), runtime.Mesh->GetIndexCount());
	}

	void TerrainRenderer::Invalidate(TerrainComponent& component)
	{
		if (component.Runtime)
			component.Runtime->Dirty = true;
	}
}