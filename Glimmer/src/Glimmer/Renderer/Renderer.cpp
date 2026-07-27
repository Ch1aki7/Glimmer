#include "glpch.h"
#include "Renderer.h"
#include "Renderer2D.h"
#include "Renderer3D.h"
#include "SkyboxRenderer.h"
#include "UniformBuffer.h"
namespace gl {

	Renderer::SceneData* Renderer::s_SceneData = new Renderer::SceneData;

	namespace {

		struct alignas(16) GPUPointLight
		{
			glm::vec4 PositionRange{ 0.0f };
			glm::vec4 ColorIntensity{ 0.0f };
		};

		struct alignas(16) GPULightEnvironment
		{
			glm::vec4 DirectionIntensity{ 0.0f };
			glm::vec4 DirectionalColor{ 0.0f };
			glm::vec4 AmbientColorIntensity{ 1.0f, 1.0f, 1.0f, 0.03f };
			glm::uvec4 LightCounts{ 0u };
			GPUPointLight PointLights[LightEnvironment::MaxPointLights];
		};

		static_assert(sizeof(GPUPointLight) == 32, "GPUPointLight must match std140 layout.");
		static_assert(sizeof(GPULightEnvironment) == 576, "Light UBO must match shader layout.");

		Ref<UniformBuffer> s_LightUniformBuffer;

	}

	void Renderer::Init()
	{
		GL_PROFILE_FUNCTION();

		RenderCommand::Init();
		Renderer2D::Init();
		Renderer3D::Init();
		s_LightUniformBuffer = UniformBuffer::Create(sizeof(GPULightEnvironment), 1);
		SkyboxRenderer::Init();
	}

	void Renderer::Shutdown() {
		s_LightUniformBuffer.reset();
		Renderer3D::Shutdown();
		SkyboxRenderer::Shutdown();
		Renderer2D::Shutdown();
	}

	void Renderer::BeginScene(OrthographicCamera& camera)
	{
		s_SceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
	}

	void Renderer::BeginScene(const glm::mat4& viewProjection)
	{
		s_SceneData->ViewProjectionMatrix = viewProjection;
	}

	void Renderer::EndScene()
	{
	}

	void Renderer::UploadLightEnvironment(const LightEnvironment& environment)
	{
		if (!s_LightUniformBuffer)
			return;

		GPULightEnvironment gpuData;
		if (environment.Directional.Enabled)
		{
			const glm::vec3 direction = glm::length(environment.Directional.Direction) > 0.0001f
				? glm::normalize(environment.Directional.Direction)
				: glm::vec3(0.0f, -1.0f, 0.0f);
			gpuData.DirectionIntensity = {
				direction,
				glm::max(environment.Directional.Intensity, 0.0f)
			};
			gpuData.DirectionalColor = glm::vec4(environment.Directional.Color, 0.0f);
			gpuData.AmbientColorIntensity = {
				environment.Directional.Color,
				glm::max(environment.Directional.AmbientIntensity, 0.0f)
			};
		}

		const uint32_t pointLightCount = glm::min(
			static_cast<uint32_t>(environment.PointLights.size()),
			LightEnvironment::MaxPointLights);
		gpuData.LightCounts.x = pointLightCount;

		for (uint32_t index = 0; index < pointLightCount; ++index)
		{
			const auto& source = environment.PointLights[index];
			gpuData.PointLights[index].PositionRange = {
				source.Position,
				glm::max(source.Range, 0.01f)
			};
			gpuData.PointLights[index].ColorIntensity = {
				source.Color,
				glm::max(source.Intensity, 0.0f)
			};
		}

		s_LightUniformBuffer->SetData(&gpuData, sizeof(GPULightEnvironment));
	}
	void Renderer::Submit(const Ref<Shader>& shader,
		const Ref<VertexArray>& vertexArray,
		const glm::mat4& transform)
	{
		shader->Bind();
		// 1. 上传场景矩阵 (PV)
		shader->UploadUniformMat4("u_ViewProjection", s_SceneData->ViewProjectionMatrix);
		// 2. 上传物体变换矩阵 (M)
		shader->UploadUniformMat4("u_Transform", transform);

		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}

}
