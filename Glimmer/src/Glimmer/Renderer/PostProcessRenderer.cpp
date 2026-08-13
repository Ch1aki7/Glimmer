#include "glpch.h"
#include "Glimmer/Renderer/PostProcessRenderer.h"

#include "Glimmer/Renderer/RenderPass.h"
#include "Glimmer/Renderer/Renderer2D.h"

#include <algorithm>

namespace gl {

	void PostProcessRenderer::Initialize(ShaderLibrary& shaderLibrary,
		const PostProcessShaderPaths& shaderPaths)
	{
		Shutdown();

		FramebufferSpecification displaySpecification;
		displaySpecification.Attachments = { { FramebufferTextureFormat::RGBA8 } };
		m_DisplayFramebuffer = Framebuffer::Create(displaySpecification);

		FramebufferSpecification bloomSpecification;
		bloomSpecification.Width = 640;
		bloomSpecification.Height = 360;
		bloomSpecification.Attachments = { { FramebufferTextureFormat::RGBA16F } };
		m_BloomFramebuffers[0] = Framebuffer::Create(bloomSpecification);
		m_BloomFramebuffers[1] = Framebuffer::Create(bloomSpecification);

		m_ToneMappingShader = shaderLibrary.Load(shaderPaths.ToneMapping);
		m_BloomExtractShader = shaderLibrary.Load(shaderPaths.BloomExtract);
		m_BloomBlurShader = shaderLibrary.Load(shaderPaths.BloomBlur);
	}

	void PostProcessRenderer::Shutdown()
	{
		m_ToneMappingShader.reset();
		m_BloomExtractShader.reset();
		m_BloomBlurShader.reset();
		m_DisplayFramebuffer.reset();
		m_BloomFramebuffers = {};
	}

	void PostProcessRenderer::Resize(uint32_t width, uint32_t height)
	{
		if (!IsInitialized() || width == 0 || height == 0)
			return;

		const auto& specification = m_DisplayFramebuffer->GetSpecification();
		if (specification.Width == width && specification.Height == height)
			return;

		m_DisplayFramebuffer->Resize(width, height);
		const uint32_t bloomWidth = std::max(width / 2u, 1u);
		const uint32_t bloomHeight = std::max(height / 2u, 1u);
		m_BloomFramebuffers[0]->Resize(bloomWidth, bloomHeight);
		m_BloomFramebuffers[1]->Resize(bloomWidth, bloomHeight);
	}

	void PostProcessRenderer::Execute(const PostProcessInput& input)
	{
		GL_CORE_ASSERT(IsInitialized(), "PostProcessRenderer is not initialized.");

		uint32_t bloomTexture = 0;
		if (m_Settings.BloomEnabled)
		{
			RenderPassSpecification bloomPass;
			bloomPass.Target = m_BloomFramebuffers[0];
			bloomPass.ClearColorValue = { 0.0f, 0.0f, 0.0f, 1.0f };
			RenderPass::Begin(bloomPass);
			m_BloomExtractShader->Bind();
			m_BloomExtractShader->UploadUniformFloat(
				"u_Threshold", m_Settings.BloomThreshold);
			m_BloomExtractShader->UploadUniformFloat(
				"u_SoftKnee", m_Settings.BloomKnee);
			m_BloomExtractShader->UploadUniformFloat(
				"u_ExposureEV", m_Settings.ExposureEV);
			Renderer2D::DrawPostProcess(
				m_BloomExtractShader, input.SceneColorTexture);
			RenderPass::End();

			bool horizontal = true;
			const int blurPasses = std::max(m_Settings.BloomBlurPasses, 1);
			for (int pass = 0; pass < blurPasses; ++pass)
			{
				const uint32_t targetIndex = horizontal ? 1u : 0u;
				const uint32_t sourceIndex = horizontal ? 0u : 1u;
				bloomPass.Target = m_BloomFramebuffers[targetIndex];
				RenderPass::Begin(bloomPass);
				m_BloomBlurShader->Bind();
				m_BloomBlurShader->UploadUniformInt(
					"u_Horizontal", horizontal ? 1 : 0);
				const auto& bloomSpecification =
					m_BloomFramebuffers[targetIndex]->GetSpecification();
				m_BloomBlurShader->UploadUniformFloat2("u_TexelSize", {
					1.0f / static_cast<float>(bloomSpecification.Width),
					1.0f / static_cast<float>(bloomSpecification.Height)
				});
				Renderer2D::DrawPostProcess(m_BloomBlurShader,
					m_BloomFramebuffers[sourceIndex]
						->GetColorAttachmentRendererID());
				RenderPass::End();
				horizontal = !horizontal;
			}
			const uint32_t finalIndex = blurPasses % 2 == 0 ? 0u : 1u;
			bloomTexture = m_BloomFramebuffers[finalIndex]
				->GetColorAttachmentRendererID();
		}

		RenderPassSpecification toneMappingPass;
		toneMappingPass.Target = m_DisplayFramebuffer;
		RenderPass::Begin(toneMappingPass);
		m_ToneMappingShader->Bind();
		// OpenGL validates sampler types for the whole linked program, even when
		// the shader branch using a sampler is disabled. Keep the cube sampler on
		// a distinct unit from every sampler2D instead of relying on GLSL's
		// default sampler value of zero.
		m_ToneMappingShader->UploadUniformInt("u_FogSkyLight", 2);
		m_ToneMappingShader->UploadUniformInt("u_BloomTexture", 3);

		glm::vec3 resolvedFogColor = m_Settings.DistanceFogColor;
		int resolvedFogColorSource = static_cast<int>(m_Settings.FogColor);
		if (m_Settings.FogColor == FogColorSource::DirectionalLight)
			resolvedFogColor = input.DirectionalLightColor;
		if (m_Settings.FogColor == FogColorSource::SkyLight)
		{
			if (input.SkyLightTexture)
			{
				input.SkyLightTexture->Bind(2);
				m_ToneMappingShader->UploadUniformFloat(
					"u_FogSkyLightIntensity",
					std::max(input.SkyLightIntensity, 0.0f));
			}
			else
				resolvedFogColorSource = static_cast<int>(FogColorSource::Manual);
		}

		m_ToneMappingShader->UploadUniformFloat(
			"u_ExposureEV", m_Settings.ExposureEV);
		m_ToneMappingShader->UploadUniformFloat(
			"u_ACESWhitePoint", m_Settings.ACESWhitePoint);
		m_ToneMappingShader->UploadUniformInt("u_BloomEnabled",
			m_Settings.BloomEnabled && bloomTexture != 0 ? 1 : 0);
		m_ToneMappingShader->UploadUniformFloat(
			"u_BloomIntensity", m_Settings.BloomIntensity);
		if (bloomTexture != 0)
			m_ToneMappingShader->BindTexture("u_BloomTexture", 3, bloomTexture);
		m_ToneMappingShader->UploadUniformInt("u_ApplyGrayscale",
			m_Settings.GrayscaleEnabled ? 1 : 0);
		m_ToneMappingShader->UploadUniformInt("u_DistanceFogEnabled",
			m_Settings.DistanceFogEnabled && input.HasCamera ? 1 : 0);
		m_ToneMappingShader->UploadUniformFloat(
			"u_DistanceFogDensity", m_Settings.DistanceFogDensity);
		m_ToneMappingShader->UploadUniformFloat(
			"u_DistanceFogStart", m_Settings.DistanceFogStart);
		m_ToneMappingShader->UploadUniformFloat(
			"u_DistanceFogEnd", m_Settings.DistanceFogEnd);
		m_ToneMappingShader->UploadUniformFloat3(
			"u_DistanceFogColor", resolvedFogColor);
		m_ToneMappingShader->UploadUniformInt("u_HeightFogEnabled",
			m_Settings.HeightFogEnabled ? 1 : 0);
		m_ToneMappingShader->UploadUniformFloat(
			"u_HeightFogBaseHeight", m_Settings.HeightFogBaseHeight);
		m_ToneMappingShader->UploadUniformFloat(
			"u_HeightFogFalloff", m_Settings.HeightFogFalloff);
		m_ToneMappingShader->UploadUniformInt(
			"u_FogColorSource", resolvedFogColorSource);
		m_ToneMappingShader->UploadUniformFloat3(
			"u_CameraPosition", input.CameraPosition);
		m_ToneMappingShader->UploadUniformMat4(
			"u_InverseViewProjection", input.InverseViewProjection);
		m_ToneMappingShader->BindTexture(
			"u_SceneDepth", 1, input.SceneDepthTexture);
		Renderer2D::DrawPostProcess(
			m_ToneMappingShader, input.SceneColorTexture);
		RenderPass::End();
	}

	uint32_t PostProcessRenderer::GetOutputTextureID() const
	{
		return m_DisplayFramebuffer
			? m_DisplayFramebuffer->GetColorAttachmentRendererID()
			: 0;
	}

}
