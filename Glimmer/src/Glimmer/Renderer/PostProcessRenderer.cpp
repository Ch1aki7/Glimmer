#include "glpch.h"
#include "Glimmer/Renderer/PostProcessRenderer.h"

#include "Glimmer/Renderer/RenderPass.h"
#include "Glimmer/Renderer/Renderer2D.h"

#include <algorithm>
#include <cctype>

namespace gl {

	void PostProcessRenderer::Initialize(ShaderLibrary& shaderLibrary,
		const PostProcessShaderPaths& shaderPaths)
	{
		Shutdown();
		m_ShaderLibrary = &shaderLibrary;

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
		if (m_ShaderLibrary)
		{
			for (const std::string& key : m_CustomPassLibraryKeys)
				if (m_ShaderLibrary->Exists(key))
					m_ShaderLibrary->Remove(key);
		}
		m_CustomPasses.clear();
		m_CustomPassShaders.clear();
		m_CustomPassLibraryKeys.clear();
		m_ShaderLibrary = nullptr;
		m_ToneMappingShader.reset();
		m_BloomExtractShader.reset();
		m_BloomBlurShader.reset();
		m_DisplayFramebuffer.reset();
		m_BloomFramebuffers = {};
		m_CustomPassFramebuffers = {};
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
		if (m_CustomPassFramebuffers[0])
		{
			m_CustomPassFramebuffers[0]->Resize(width, height);
			m_CustomPassFramebuffers[1]->Resize(width, height);
		}
	}

	void PostProcessRenderer::EnsureCustomPassFramebuffers(
		uint32_t width, uint32_t height)
	{
		if (m_CustomPassFramebuffers[0])
			return;

		FramebufferSpecification specification;
		specification.Width = std::max(width, 1u);
		specification.Height = std::max(height, 1u);
		specification.Attachments = { { FramebufferTextureFormat::RGBA16F } };
		m_CustomPassFramebuffers[0] = Framebuffer::Create(specification);
		m_CustomPassFramebuffers[1] = Framebuffer::Create(specification);
	}

	bool PostProcessRenderer::AddCustomPass(
		const std::filesystem::path& shaderPath)
	{
		std::string extension = shaderPath.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
		if (!m_ShaderLibrary || extension != ".glsl")
			return false;

		std::error_code error;
		std::filesystem::path normalized =
			std::filesystem::weakly_canonical(shaderPath, error);
		if (error || !std::filesystem::is_regular_file(normalized, error))
			return false;
		for (const auto& pass : m_CustomPasses)
			if (pass.ShaderPath == normalized)
				return false;

		const std::string libraryKey =
			"Post Process / " + normalized.generic_string();
		if (m_ShaderLibrary->Exists(libraryKey))
			return false;

		Ref<Shader> shader = m_ShaderLibrary->Load(
			libraryKey, normalized.string());
		if (!shader)
			return false;

		m_CustomPasses.push_back({
			normalized.stem().string(), normalized, true
		});
		m_CustomPassShaders.push_back(std::move(shader));
		m_CustomPassLibraryKeys.push_back(libraryKey);
		const auto& displaySpecification =
			m_DisplayFramebuffer->GetSpecification();
		EnsureCustomPassFramebuffers(
			displaySpecification.Width, displaySpecification.Height);
		return true;
	}

	bool PostProcessRenderer::RemoveCustomPass(size_t index)
	{
		if (index >= m_CustomPasses.size())
			return false;
		if (m_ShaderLibrary
			&& m_ShaderLibrary->Exists(m_CustomPassLibraryKeys[index]))
			m_ShaderLibrary->Remove(m_CustomPassLibraryKeys[index]);
		m_CustomPasses.erase(m_CustomPasses.begin() + index);
		m_CustomPassShaders.erase(m_CustomPassShaders.begin() + index);
		m_CustomPassLibraryKeys.erase(m_CustomPassLibraryKeys.begin() + index);
		if (m_CustomPasses.empty())
			m_CustomPassFramebuffers = {};
		return true;
	}

	bool PostProcessRenderer::MoveCustomPass(size_t fromIndex, size_t toIndex)
	{
		if (fromIndex >= m_CustomPasses.size()
			|| toIndex >= m_CustomPasses.size())
			return false;
		if (fromIndex == toIndex)
			return true;
		std::swap(m_CustomPasses[fromIndex], m_CustomPasses[toIndex]);
		std::swap(m_CustomPassShaders[fromIndex], m_CustomPassShaders[toIndex]);
		std::swap(m_CustomPassLibraryKeys[fromIndex],
			m_CustomPassLibraryKeys[toIndex]);
		return true;
	}

	bool PostProcessRenderer::SetCustomPassEnabled(size_t index, bool enabled)
	{
		if (index >= m_CustomPasses.size())
			return false;
		m_CustomPasses[index].Enabled = enabled;
		return true;
	}

	void PostProcessRenderer::Execute(const PostProcessInput& input)
	{
		GL_CORE_ASSERT(IsInitialized(), "PostProcessRenderer is not initialized.");
		const auto& displaySpecification =
			m_DisplayFramebuffer->GetSpecification();
		const glm::vec2 displayResolution{
			static_cast<float>(displaySpecification.Width),
			static_cast<float>(displaySpecification.Height)
		};
		if (!m_CustomPasses.empty())
			EnsureCustomPassFramebuffers(
				displaySpecification.Width, displaySpecification.Height);

		uint32_t resolvedSceneColor = input.SceneColorTexture;
		uint32_t executedCustomPasses = 0;
		for (size_t index = 0; index < m_CustomPasses.size(); ++index)
		{
			if (!m_CustomPasses[index].Enabled || !m_CustomPassShaders[index])
				continue;

			const uint32_t targetIndex = executedCustomPasses % 2u;
			RenderPassSpecification customPass;
			customPass.Target = m_CustomPassFramebuffers[targetIndex];
			customPass.ClearColorValue = { 0.0f, 0.0f, 0.0f, 1.0f };
			RenderPass::Begin(customPass);

			const Ref<Shader>& shader = m_CustomPassShaders[index];
			shader->Bind();
			shader->UploadUniformInt("u_SceneDepth", 1);
			shader->UploadUniformInt("u_HasCamera", input.HasCamera ? 1 : 0);
			shader->UploadUniformFloat3(
				"u_CameraPosition", input.CameraPosition);
			shader->UploadUniformMat4(
				"u_InverseViewProjection", input.InverseViewProjection);
			if (input.SceneDepthTexture != 0)
				shader->BindTexture(
					"u_SceneDepth", 1, input.SceneDepthTexture);
			Renderer2D::DrawPostProcess(
				shader, resolvedSceneColor, displayResolution);
			RenderPass::End();

			resolvedSceneColor = m_CustomPassFramebuffers[targetIndex]
				->GetColorAttachmentRendererID();
			++executedCustomPasses;
		}

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
				m_BloomExtractShader, resolvedSceneColor, {
					static_cast<float>(m_BloomFramebuffers[0]
						->GetSpecification().Width),
					static_cast<float>(m_BloomFramebuffers[0]
						->GetSpecification().Height)
				});
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
						->GetColorAttachmentRendererID(), {
							static_cast<float>(bloomSpecification.Width),
							static_cast<float>(bloomSpecification.Height)
						});
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
			m_ToneMappingShader, resolvedSceneColor, displayResolution);
		RenderPass::End();
	}

	uint32_t PostProcessRenderer::GetOutputTextureID() const
	{
		return m_DisplayFramebuffer
			? m_DisplayFramebuffer->GetColorAttachmentRendererID()
			: 0;
	}

}
