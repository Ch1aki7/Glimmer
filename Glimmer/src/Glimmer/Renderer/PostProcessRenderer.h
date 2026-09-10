#pragma once

#include "Glimmer/Core/Core.h"
#include "Glimmer/Renderer/Framebuffer.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/TextureCube.h"

#include <array>
#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace gl {

	enum class FogColorSource : int
	{
		Manual = 0,
		SkyLight = 1,
		DirectionalLight = 2
	};

	struct PostProcessSettings
	{
		bool GrayscaleEnabled = false;
		float ExposureEV = 0.0f;
		float ACESWhitePoint = 11.2f;

		bool BloomEnabled = true;
		float BloomThreshold = 1.0f;
		float BloomKnee = 0.5f;
		float BloomIntensity = 0.08f;
		int BloomBlurPasses = 6;

		bool DistanceFogEnabled = false;
		float DistanceFogDensity = 0.012f;
		float DistanceFogStart = 60.0f;
		float DistanceFogEnd = 260.0f;
		glm::vec3 DistanceFogColor = { 0.55f, 0.65f, 0.75f };
		bool HeightFogEnabled = true;
		float HeightFogBaseHeight = 18.0f;
		float HeightFogFalloff = 0.035f;
		FogColorSource FogColor = FogColorSource::Manual;
	};

	struct PostProcessInput
	{
		uint32_t SceneColorTexture = 0;
		uint32_t SceneDepthTexture = 0;
		bool HasCamera = false;
		glm::mat4 InverseViewProjection{ 1.0f };
		glm::vec3 CameraPosition{ 0.0f };
		Ref<TextureCube> SkyLightTexture;
		float SkyLightIntensity = 1.0f;
		glm::vec3 DirectionalLightColor{ 1.0f };
	};

	struct PostProcessShaderPaths
	{
		const char* ToneMapping = "assets/shaders/ToneMapping.glsl";
		const char* BloomExtract = "assets/shaders/BloomExtract.glsl";
		const char* BloomBlur = "assets/shaders/BloomBlur.glsl";
	};

	struct CustomPostProcessPass
	{
		std::string Name;
		std::filesystem::path ShaderPath;
		bool Enabled = true;
	};

	class PostProcessRenderer
	{
	public:
		void Initialize(ShaderLibrary& shaderLibrary,
			const PostProcessShaderPaths& shaderPaths = {});
		void Shutdown();
		void Resize(uint32_t width, uint32_t height);
		void Execute(const PostProcessInput& input);
		bool AddCustomPass(const std::filesystem::path& shaderPath);
		bool RemoveCustomPass(size_t index);
		bool MoveCustomPass(size_t fromIndex, size_t toIndex);
		bool SetCustomPassEnabled(size_t index, bool enabled);
		const std::vector<CustomPostProcessPass>& GetCustomPasses() const
		{
			return m_CustomPasses;
		}

		PostProcessSettings& GetSettings() { return m_Settings; }
		const PostProcessSettings& GetSettings() const { return m_Settings; }
		uint32_t GetOutputTextureID() const;
		bool IsInitialized() const { return m_DisplayFramebuffer != nullptr; }

	private:
		void EnsureCustomPassFramebuffers(uint32_t width, uint32_t height);

		PostProcessSettings m_Settings;
		Ref<Framebuffer> m_DisplayFramebuffer;
		std::array<Ref<Framebuffer>, 2> m_BloomFramebuffers;
		std::array<Ref<Framebuffer>, 2> m_CustomPassFramebuffers;
		Ref<Shader> m_ToneMappingShader;
		Ref<Shader> m_BloomExtractShader;
		Ref<Shader> m_BloomBlurShader;
		ShaderLibrary* m_ShaderLibrary = nullptr;
		std::vector<CustomPostProcessPass> m_CustomPasses;
		std::vector<Ref<Shader>> m_CustomPassShaders;
		std::vector<std::string> m_CustomPassLibraryKeys;
	};

}
