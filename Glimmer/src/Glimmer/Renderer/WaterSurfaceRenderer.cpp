#include "glpch.h"
#include "WaterSurfaceRenderer.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/TerrainRenderer.h"
#include "Glimmer/Renderer/FrustumCulling.h"
#include "Glimmer/Renderer/EnvironmentLighting.h"
#include "Glimmer/Renderer/GPUTimer.h"
#include "Glimmer/Asset/AssetManager.h"
#include <chrono>
#include <map>
#include "Glimmer/Terrain/Terrain.h"
#include "Glimmer/Terrain/TerrainChunkLayout.h"

namespace gl {
	namespace {
		Ref<Framebuffer> s_Background;
        WaterSurfaceRenderer::Statistics s_Statistics;
        uint64_t s_PeakOwnedBytes=0;
		Ref<Shader> s_Shader;
        std::chrono::steady_clock::time_point s_ShaderRetryTime{};
        Ref<Texture2D> s_DefaultNormal, s_DefaultNoise;
        std::array<Ref<TerrainMesh>,3> s_WaterMeshes;
        Ref<GPUTimer> s_CopyTimer, s_DrawTimer;
        struct DetailEntry {
            Ref<Texture2D> Texture;
            std::filesystem::path Path;
            std::filesystem::file_time_type Stamp{};
            TextureSemantic Semantic=TextureSemantic::Data;
            TextureColorSpace ColorSpace=TextureColorSpace::Linear;
            std::chrono::steady_clock::time_point NextPoll{};
            std::string Error;
            bool Seen=false;
        };
        std::map<std::pair<uint64_t,bool>,DetailEntry> s_Details;
        uint64_t TextureBytes(const Ref<Texture2D>& texture) {
            if(!texture) return 0;
            uint64_t channels=texture->GetFormat()==TextureFormat::R8?1: texture->GetFormat()==TextureFormat::RGB8?3:4;
            uint64_t bytes=0, w=texture->GetWidth(),h=texture->GetHeight();
            do { bytes+=w*h*channels; if(texture->GetSpecification().MinFilter!=TextureFilter::LinearMipmapLinear) break;
                if(w==1&&h==1) break; w=std::max(uint64_t(1),w/2);h=std::max(uint64_t(1),h/2); } while(true);
            return bytes;
        }
        void RecordOwned(uint64_t candidateExtra=0) {
            s_Statistics.BackgroundBytes=s_Background?uint64_t(s_Background->GetSpecification().Width)*s_Background->GetSpecification().Height*12:0;
            s_Statistics.DetailBytes=TextureBytes(s_DefaultNormal)+TextureBytes(s_DefaultNoise);
            s_Statistics.MeshBytes=0;s_Statistics.CachedMeshes=0;
            for(const auto& pair:s_Details) s_Statistics.DetailBytes+=TextureBytes(pair.second.Texture);
            for(const auto& mesh:s_WaterMeshes) if(mesh) {
                ++s_Statistics.CachedMeshes;uint64_t n=mesh->GetGridSize();
                s_Statistics.MeshBytes+=24*((n+1)*(n+1)+4*(n+1))+4*(6*n*n+24*n);
            }
            s_PeakOwnedBytes=std::max(s_PeakOwnedBytes,s_Statistics.BackgroundBytes+s_Statistics.DetailBytes+s_Statistics.MeshBytes+candidateExtra);
            s_Statistics.PeakOwnedBytes=s_PeakOwnedBytes;
        }
        void CreateBuiltins() {
            if(s_DefaultNormal) return;
            constexpr uint32_t size=256;
            std::vector<uint8_t> normals(size*size*3),noise(size*size);
            // Integer Fourier modes are periodic in both axes. No external resource or simulation readback.
            for(uint32_t y=0;y<size;++y) for(uint32_t x=0;x<size;++x) {
                float gx=0,gy=0,value=0;
                for(uint32_t i=0;i<18;++i) {
                    const int kx=int((i*7+3)%17)-8, ky=int((i*11+5)%19)-9;
                    const float amplitude=0.12f/(1.0f+float(kx*kx+ky*ky));
                    const float phase=6.2831853f*(float(kx)*x/size+float(ky)*y/size)+i*2.3999632f;
                    gx+=amplitude*kx*std::cos(phase);gy+=amplitude*ky*std::cos(phase);
                    value+=std::sin(phase)/(1.0f+float(i)*0.4f);
                }
                const glm::vec3 n=glm::normalize(glm::vec3(-gx*9,-gy*9,1));
                size_t index=size_t(y)*size+x;
                for(int c=0;c<3;++c) normals[index*3+c]=uint8_t(std::clamp(n[c]*0.5f+0.5f,0.0f,1.0f)*255+0.5f);
                noise[index]=uint8_t(std::clamp(0.5f+value*0.17f,0.0f,1.0f)*255+0.5f);
            }
            TextureSpecification t; t.Width=t.Height=size;t.Format=TextureFormat::RGB8;
            t.MinFilter=TextureFilter::LinearMipmapLinear;t.MagFilter=TextureFilter::Linear;
            s_DefaultNormal=Texture2D::Create(t);s_DefaultNormal->SetData(normals.data(),uint32_t(normals.size()));
            t.Format=TextureFormat::R8;s_DefaultNoise=Texture2D::Create(t);s_DefaultNoise->SetData(noise.data(),uint32_t(noise.size()));
        }
        Ref<Texture2D> ResolveDetail(AssetHandle handle,bool normal,std::string& warning) {
            const auto fallback=normal?s_DefaultNormal:s_DefaultNoise;
            if(uint64_t(handle)==0) return fallback;
            auto& entry=s_Details[std::make_pair(uint64_t(handle),normal)];entry.Seen=true;
            const auto now=std::chrono::steady_clock::now();
            if(now>=entry.NextPoll) {
                entry.NextPoll=now+std::chrono::milliseconds(250);
                const auto meta=AssetManager::GetMetadata(handle);
                const auto path=AssetManager::GetFileSystemPath(handle);
                std::error_code ec; auto stamp=std::filesystem::last_write_time(path,ec);
                const auto semantic=normal?TextureSemantic::Normal:TextureSemantic::Data;
                if(entry.Path!=path||entry.Stamp!=stamp||entry.Semantic!=meta.Semantic||entry.ColorSpace!=meta.ColorSpace
                    ||(!entry.Texture&&entry.Error.empty())) {
                    entry.Path=path;entry.Stamp=stamp;entry.Semantic=meta.Semantic;entry.ColorSpace=meta.ColorSpace;
                    if(meta.Type!=AssetType::Texture2D||meta.ColorSpace!=TextureColorSpace::Linear||meta.Semantic!=semantic)
                        entry.Error=normal?"Water normal requires Linear/Normal asset metadata.":"Water foam requires Linear/Data asset metadata.";
                    else if(ec) entry.Error="Water detail asset is missing; retaining previous texture or built-in.";
                    else {
                        auto candidate=Texture2D::Create(path.string(),TextureColorSpace::Linear,TextureFilter::LinearMipmapLinear,TextureFilter::Linear);
                        if(candidate) RecordOwned(TextureBytes(candidate));
                        if(candidate && (!normal || candidate->GetFormat()==TextureFormat::RGB8 || candidate->GetFormat()==TextureFormat::RGBA8)) {
                            entry.Texture=std::move(candidate);entry.Error.clear();
                        } else entry.Error="Water detail decode/normal channels rejected; retaining previous texture or built-in.";
                    }
                }
            }
            if(!entry.Error.empty()) warning=entry.Error;
            return entry.Texture?entry.Texture:fallback;
        }
		WaterSurfaceSettings s_Settings;

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
		s_DefaultNormal.reset();s_DefaultNoise.reset();s_WaterMeshes={};s_Details.clear();
		s_CopyTimer.reset();s_DrawTimer.reset();s_PeakOwnedBytes=0;
	}

	void WaterSurfaceRenderer::Render(const Ref<Framebuffer>& target,
		const std::vector<WaterSurfaceInstance>& instances,
		const glm::mat4& viewProjection, const glm::vec3& cameraPosition)
	{
		s_Statistics = {};
        RecordOwned();
		if (!s_Settings.Enabled || !target || instances.empty()
			|| TerrainRenderer::IsLODVisualizationEnabled()
			|| TerrainRenderer::GetAuthoringVisualizationMode() != TerrainRenderer::AuthoringVisualizationMode::None
			|| TerrainRenderer::GetHydrologyVisualizationMode() != TerrainRenderer::HydrologyVisualizationMode::None
			|| TerrainRenderer::GetClimateVisualizationMode() != TerrainRenderer::ClimateVisualizationMode::None)
			return;
        bool visibleWater=false;
        for(const auto& instance:instances) if(instance.Terrain&&instance.Terrain->Runtime&&instance.Terrain->Runtime->GPUHydrology) {
            const auto& appearance=instance.Terrain->Specification.Water;
            const auto error=ValidateWaterSurfaceAppearance(appearance);
            if(!error.empty()) s_Statistics.ResourceWarning=error;
            else if(appearance.Enabled) visibleWater=true;
        }
        if(!visibleWater) return;
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
			if (!std::filesystem::exists(path)) { s_Statistics.ResourceWarning="Water shader is missing.";return; }
			s_Shader = Shader::Create(path.string(),false);
            s_ShaderRetryTime=std::chrono::steady_clock::now()+std::chrono::seconds(1);
		}
		s_Shader->ReloadIfChanged();
        if(!s_Shader->GetVersion()&&std::chrono::steady_clock::now()>=s_ShaderRetryTime) {
            s_Shader->Reload();s_ShaderRetryTime=std::chrono::steady_clock::now()+std::chrono::seconds(1);
        }
        if(!s_Shader->GetLastReloadResult().Success) s_Statistics.ResourceWarning="Water shader: "+s_Shader->GetLastReloadResult().Message;
        if (!s_Shader->GetVersion()) return;
        if (!s_CopyTimer) { s_CopyTimer=GPUTimer::Create();s_DrawTimer=GPUTimer::Create(); }
        s_Statistics.GpuTimingAvailable=s_CopyTimer->TryGetElapsedMilliseconds(s_Statistics.CopyMilliseconds);
        s_Statistics.GpuTimingAvailable=s_DrawTimer->TryGetElapsedMilliseconds(s_Statistics.DrawMilliseconds)&&s_Statistics.GpuTimingAvailable;
        CreateBuiltins();
        for(auto& pair:s_Details) pair.second.Seen=false;
        if (!s_Background)
		{
			FramebufferSpecification background;
			background.Width = spec.Width; background.Height = spec.Height;
			background.Attachments = { { FramebufferTextureFormat::RGBA16F },
				{ FramebufferTextureFormat::Depth24Stencil8 } };
			s_Background = Framebuffer::Create(background);
		}
        else {
            if(s_Background->GetSpecification().Width!=spec.Width||s_Background->GetSpecification().Height!=spec.Height)
                RecordOwned(uint64_t(spec.Width)*spec.Height*12);
            s_Background->Resize(spec.Width,spec.Height);
        }
		s_CopyTimer->Begin();
		const bool copied=target->CopyColorAndDepthTo(*s_Background);
		s_CopyTimer->End();
		if (!copied) return;
		s_DrawTimer->Begin();
		s_Statistics.SnapshotReady = true;
		s_Statistics.BackgroundBytes=uint64_t(spec.Width)*spec.Height*12;
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
		s_Shader->UploadUniformInt("u_SmoothSampling",s_Settings.SmoothSampling?1:0);
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
			const auto& terrain = TerrainRenderer::GetSurfaceSpecification(*instance.Terrain);
			if (!std::isfinite(terrain.HeightScale)) continue;
            const auto& appearance=instance.Terrain->Specification.Water;
            auto waterError=ValidateWaterSurfaceAppearance(appearance);
            if(!waterError.empty()) { s_Statistics.ResourceWarning=waterError;continue; }
            if(!appearance.Enabled) continue;
			const float worldSize = ClampTerrainWorldSize(terrain.WorldSize);
			if (!std::isfinite(glm::determinant(instance.Transform))
				|| std::abs(glm::determinant(instance.Transform)) < 1e-8f) continue;
			s_Shader->UploadUniformMat4("u_Transform", instance.Transform);
			s_Shader->UploadUniformFloat("u_HeightScale", terrain.HeightScale);
			s_Shader->UploadUniformInt("u_TerrainDataVersion",
				terrain.Procedural ? std::clamp(terrain.DataVersion, 1u, 2u) : 1u);
			s_Shader->UploadUniformFloat("u_WorldSize", worldSize);
			s_Shader->UploadUniformInt("u_EntityID", instance.EntityID);
			s_Shader->UploadUniformFloat("u_Time", runtime.GPUEnvironment
				? static_cast<float>(std::fmod(runtime.GPUEnvironment->GetStatistics().SimulatedTime, 1024.0)) : 0.0f);
			s_Shader->BindTexture("u_Height", 0, runtime.HeightMap->GetRendererID());
			s_Shader->BindTexture("u_Water", 1, runtime.GPUHydrology->GetWaterTexture()->GetRendererID());
			s_Shader->BindTexture("u_Velocity", 2, runtime.GPUHydrology->GetVelocityTexture()->GetRendererID());
			s_Shader->BindTexture("u_Sediment", 3, runtime.GPUHydrology->GetSedimentTexture()->GetRendererID());
            s_Shader->UploadUniformFloat("u_Absorption",std::clamp(appearance.Absorption*s_Settings.Absorption,0.0f,10.0f));
            s_Shader->UploadUniformFloat("u_RefractionPixels",std::clamp(appearance.RefractionPixels*s_Settings.RefractionPixels/5.0f,0.0f,32.0f));
            s_Shader->UploadUniformFloat("u_FoamStrength",std::clamp(appearance.FoamStrength*s_Settings.FoamStrength/0.35f,0.0f,1.0f));
            s_Shader->UploadUniformFloat("u_SedimentTint",std::clamp(appearance.SedimentTint*s_Settings.SedimentTint/0.4f,0.0f,4.0f));
            s_Shader->UploadUniformFloat("u_NormalStrength",s_Settings.DetailNormals?appearance.NormalStrength:0);
            s_Shader->UploadUniformFloat("u_WaveLength",appearance.WaveLength);
            s_Shader->UploadUniformFloat("u_FlowStrength",appearance.FlowStrength);
            s_Shader->UploadUniformFloat("u_ShoreFoam",s_Settings.ShoreFoam?appearance.ShoreFoam:0);
            s_Shader->UploadUniformFloat("u_ShoreWidth",appearance.ShoreWidth);
            s_Shader->UploadUniformFloat("u_Roughness",appearance.Roughness);
            s_Shader->UploadUniformInt("u_NormalDirectX",appearance.NormalDirectX?1:0);
            std::string normalWarning,foamWarning;
            auto normal=ResolveDetail(appearance.NormalTexture,true,normalWarning);
            auto foam=ResolveDetail(appearance.FoamTexture,false,foamWarning);
            if(!normalWarning.empty()||!foamWarning.empty()) {
                ++s_Statistics.TextureFallbacks;s_Statistics.ResourceWarning=normalWarning.empty()?foamWarning:normalWarning;
            }
            s_Shader->BindTexture("u_DetailNormal",9,normal->GetRendererID());
            s_Shader->BindTexture("u_FoamNoise",10,foam->GetRendererID());
            if(s_Settings.IndependentMesh&&!s_WaterMeshes[appearance.MeshQuality])
                s_WaterMeshes[appearance.MeshQuality]=CreateRef<TerrainMesh>(32u<<appearance.MeshQuality);
			const auto chunks = TerrainChunkLayout::Build(worldSize, runtime.Mesh->GetGridSize());
			for (size_t index = 0; index < chunks.size(); ++index)
			{
				// Include the shader's maximum visual water depth in the bound.
				const auto& chunk = chunks[index];
				const float halfSize = chunk.WorldSize * 0.5f;
				if (!FrustumCulling::IntersectsClipFrustum(
					{ chunk.LocalOffset.x - halfSize,
						std::min(0.0f, terrain.HeightScale),
						chunk.LocalOffset.y - halfSize },
					{ chunk.LocalOffset.x + halfSize,
						std::max(0.0f, terrain.HeightScale) + 1000.0f,
						chunk.LocalOffset.y + halfSize },
					instance.Transform, viewProjection)) continue;
				const uint32_t lod = index < runtime.ChunkLODLevels.size()
					? std::min(runtime.ChunkLODLevels[index], 2u) : 0u;
				const auto& mesh = s_Settings.IndependentMesh?s_WaterMeshes[appearance.MeshQuality]:runtime.LODMeshes[lod];
				if (!mesh) continue;
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
        s_DrawTimer->End();
        for(auto i=s_Details.begin();i!=s_Details.end();) {
            if(!i->second.Seen) i=s_Details.erase(i);else ++i;
        }
        RecordOwned();
        RenderCommand::SetCullMode(CullMode::None);
    }
}
