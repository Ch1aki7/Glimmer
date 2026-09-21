#include "glpch.h"
#include "WaterSurfaceRenderer.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/TerrainRenderer.h"
#include "Glimmer/Renderer/EnvironmentLighting.h"
#include "Glimmer/Terrain/Terrain.h"
#include "Glimmer/Terrain/TerrainChunkLayout.h"

namespace gl {
	namespace {
		Ref<Framebuffer> s_Background;
		Ref<Shader> s_Shader;
		WaterSurfaceSettings s_Settings;
		WaterSurfaceRenderer::Statistics s_Statistics;
		float Bounded(float value, float fallback, float maximum)
		{
			return std::isfinite(value) ? std::clamp(value, 0.0f, maximum) : fallback;
		}
	}
	void WaterSurfaceRenderer::SetSettings(const WaterSurfaceSettings& settings)
	{
		s_Settings = settings;
		s_Settings.Absorption = Bounded(settings.Absorption, 1.0f, 10.0f);
		s_Settings.RefractionPixels = Bounded(settings.RefractionPixels, 5.0f, 32.0f);
		s_Settings.FoamStrength = Bounded(settings.FoamStrength, 0.35f, 1.0f);
		s_Settings.SedimentTint = Bounded(settings.SedimentTint, 0.4f, 4.0f);
		s_Settings.ShoreWetness = Bounded(settings.ShoreWetness, 0.6f, 1.0f);
	}
	const WaterSurfaceSettings& WaterSurfaceRenderer::GetSettings() { return s_Settings; }
	WaterSurfaceRenderer::Statistics WaterSurfaceRenderer::GetStatistics() { return s_Statistics; }
	void WaterSurfaceRenderer::Shutdown()
	{
		s_Background.reset();
		s_Shader.reset();
		s_Statistics = {};
	}

	void WaterSurfaceRenderer::Render(const Ref<Framebuffer>& target,
		const std::vector<WaterSurfaceInstance>& instances,
		const glm::mat4& viewProjection, const glm::vec3& cameraPosition)
	{
		s_Statistics = {};
		if (!s_Settings.Enabled || !target || instances.empty()
			|| TerrainRenderer::IsLODVisualizationEnabled()
			|| TerrainRenderer::GetHydrologyVisualizationMode() != TerrainRenderer::HydrologyVisualizationMode::None
			|| TerrainRenderer::GetClimateVisualizationMode() != TerrainRenderer::ClimateVisualizationMode::None)
			return;
		const auto& spec = target->GetSpecification();
		// Full Scene MRT is required for picking and normal/depth coherence.
		if (spec.Samples != 1 || spec.Attachments.size() != 4
			|| spec.Attachments[0].Format != FramebufferTextureFormat::RGBA16F
			|| spec.Attachments[1].Format != FramebufferTextureFormat::RED_INTEGER
			|| spec.Attachments[2].Format != FramebufferTextureFormat::RGBA16F
			|| spec.Attachments[3].Format != FramebufferTextureFormat::Depth24Stencil8)
			return;
		if (!s_Shader)
		{
			const std::filesystem::path path = "assets/shaders/WaterSurface.glsl";
			if (!std::filesystem::exists(path)) return;
			s_Shader = Shader::Create(path.string());
		}
		s_Shader->ReloadIfChanged();
		if (!s_Shader->GetVersion()) return;
		if (!s_Background)
		{
			FramebufferSpecification background;
			background.Width = spec.Width; background.Height = spec.Height;
			background.Attachments = { { FramebufferTextureFormat::RGBA16F },
				{ FramebufferTextureFormat::Depth24Stencil8 } };
			s_Background = Framebuffer::Create(background);
		}
		else s_Background->Resize(spec.Width, spec.Height);
		if (!target->CopyColorAndDepthTo(*s_Background)) return;
		s_Statistics.SnapshotReady = true;
		target->Bind();
		RenderCommand::SetBlendEnabled(false);
		RenderCommand::SetDepthWriteEnabled(true);
		RenderCommand::SetDepthFunction(DepthFunction::Less);
		RenderCommand::SetCullMode(CullMode::None);
		s_Shader->Bind();
		s_Shader->UploadUniformMat4("u_ViewProjection", viewProjection);
		s_Shader->UploadUniformMat4("u_InverseViewProjection", glm::inverse(viewProjection));
		s_Shader->UploadUniformFloat3("u_CameraPosition", cameraPosition);
		s_Shader->UploadUniformFloat2("u_Resolution", { static_cast<float>(spec.Width), static_cast<float>(spec.Height) });
		s_Shader->UploadUniformFloat("u_Absorption", s_Settings.Absorption);
		s_Shader->UploadUniformFloat("u_RefractionPixels", s_Settings.RefractionPixels);
		s_Shader->UploadUniformFloat("u_FoamStrength", s_Settings.FoamStrength);
		s_Shader->UploadUniformFloat("u_SedimentTint", s_Settings.SedimentTint);
		s_Shader->BindTexture("u_SceneColor", 4, s_Background->GetColorAttachmentRendererID());
		s_Shader->BindTexture("u_SceneDepth", 5, s_Background->GetDepthAttachmentRendererID());
		EnvironmentLighting::BindForLighting(s_Shader, 6, 7, 8);
		for (const auto& instance : instances)
		{
			if (!instance.Terrain || !instance.Terrain->Runtime) continue;
			const auto& runtime = *instance.Terrain->Runtime;
			if (!runtime.GPUHydrology || !runtime.HeightMap || !runtime.Mesh) continue;
			const auto& terrain = instance.Terrain->Specification;
			if (!std::isfinite(terrain.HeightScale)) continue;
			const float worldSize = ClampTerrainWorldSize(terrain.WorldSize);
			if (!std::isfinite(glm::determinant(instance.Transform))
				|| std::abs(glm::determinant(instance.Transform)) < 1e-8f) continue;
			s_Shader->UploadUniformMat4("u_Transform", instance.Transform);
			s_Shader->UploadUniformFloat("u_HeightScale", terrain.HeightScale);
			s_Shader->UploadUniformFloat("u_WorldSize", worldSize);
			s_Shader->UploadUniformInt("u_EntityID", instance.EntityID);
			s_Shader->UploadUniformFloat("u_Time", runtime.GPUEnvironment
				? static_cast<float>(std::fmod(runtime.GPUEnvironment->GetStatistics().SimulatedTime, 1024.0)) : 0.0f);
			s_Shader->BindTexture("u_Height", 0, runtime.HeightMap->GetRendererID());
			s_Shader->BindTexture("u_Water", 1, runtime.GPUHydrology->GetWaterTexture()->GetRendererID());
			s_Shader->BindTexture("u_Velocity", 2, runtime.GPUHydrology->GetVelocityTexture()->GetRendererID());
			s_Shader->BindTexture("u_Sediment", 3, runtime.GPUHydrology->GetSedimentTexture()->GetRendererID());
			const auto chunks = TerrainChunkLayout::Build(worldSize, runtime.Mesh->GetGridSize());
			for (size_t index = 0; index < chunks.size(); ++index)
			{
				// Water has no CPU height bound. Do not cull it with dry-ground bounds.
				const auto& mesh = runtime.LODMeshes[std::min(runtime.ChunkLODLevels[index], 2u)];
				if (!mesh) continue;
				const auto& chunk = chunks[index];
				s_Shader->UploadUniformFloat2("u_UVOffset", chunk.UVOffset);
				s_Shader->UploadUniformFloat2("u_UVScale", chunk.UVScale);
				s_Shader->UploadUniformFloat2("u_LocalOffset", chunk.LocalOffset);
				s_Shader->UploadUniformFloat("u_LocalScale", chunk.WorldSize / mesh->GetGridSize());
				// Surface indices precede the terrain's vertical skirts.
				const uint32_t count = mesh->GetGridSize() * mesh->GetGridSize() * 6;
				RenderCommand::DrawIndexed(mesh->GetVertexArray(), count);
				++s_Statistics.DrawCalls;
				s_Statistics.Triangles += count / 3;
			}
		}
		RenderCommand::SetCullMode(CullMode::None);
	}
}
