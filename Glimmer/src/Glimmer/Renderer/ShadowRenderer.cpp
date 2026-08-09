#include "glpch.h"
#include "ShadowRenderer.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/FrameBuffer.h"
#include "Glimmer/Renderer/Mesh.h"
#include "Glimmer/Renderer/Model.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Scene/Components.h"
#include "Glimmer/Terrain/Terrain.h"

#include <glm/gtc/matrix_transform.hpp>

namespace gl {
	namespace {
		struct ShadowRendererData
		{
			Ref<Framebuffer> Framebuffer;
			Ref<Shader> DepthShader;
			glm::mat4 LightViewProjection{ 1.0f };
			uint32_t Resolution = 0;
			float Bias = 0.0015f;
			bool PassActive = false;
			bool Enabled = false;
		};

		ShadowRendererData s_Data;
	}

	void ShadowRenderer::Shutdown()
	{
		s_Data.PassActive = false;
		s_Data.Enabled = false;
		s_Data.DepthShader.reset();
		s_Data.Framebuffer.reset();
		s_Data.Resolution = 0;
	}

	bool ShadowRenderer::BeginDirectional(
		const glm::vec3& lightDirection,
		const glm::vec3& focusPosition,
		uint32_t resolution,
		float distance,
		float bias)
	{
		Disable();
		const AssetHandle shaderHandle =
			AssetManager::ImportAsset("assets/shaders/ShadowDepth.glsl");
		s_Data.DepthShader = AssetManager::GetShader(shaderHandle);
		if (!s_Data.DepthShader)
			return false;

		resolution = std::clamp(resolution, 512u, 4096u);
		if (!s_Data.Framebuffer || s_Data.Resolution != resolution)
		{
			FramebufferSpecification specification;
			specification.Width = resolution;
			specification.Height = resolution;
			specification.Attachments = { { FramebufferTextureFormat::Depth32F } };
			s_Data.Framebuffer = Framebuffer::Create(specification);
			s_Data.Resolution = resolution;
		}

		const glm::vec3 direction = glm::length(lightDirection) > 0.0001f
			? glm::normalize(lightDirection) : glm::vec3(0.0f, -1.0f, 0.0f);
		distance = std::clamp(distance, 10.0f, 500.0f);
		const glm::vec3 up = std::abs(glm::dot(direction, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.98f
			? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		const glm::mat4 lightView = glm::lookAt(
			focusPosition - direction * distance, focusPosition, up);
		const glm::mat4 lightProjection = glm::ortho(
			-distance, distance, -distance, distance, 0.1f, distance * 2.5f);
		s_Data.LightViewProjection = lightProjection * lightView;
		s_Data.Bias = std::clamp(bias, 0.00001f, 0.05f);

		s_Data.Framebuffer->Bind();
		RenderCommand::SetBlendEnabled(false);
		RenderCommand::SetDepthWriteEnabled(true);
		RenderCommand::SetDepthFunction(DepthFunction::Less);
		RenderCommand::ClearDepth();
		s_Data.DepthShader->ReloadIfChanged();
		s_Data.DepthShader->Bind();
		s_Data.DepthShader->UploadUniformMat4(
			"u_LightViewProjection", s_Data.LightViewProjection);
		s_Data.DepthShader->UploadUniformInt("u_HeightMap", 0);
		s_Data.PassActive = true;
		return true;
	}

	void ShadowRenderer::SubmitModel(AssetHandle modelHandle, const glm::mat4& transform)
	{
		if (!s_Data.PassActive)
			return;
		const Ref<Model> model = AssetManager::GetModel(modelHandle);
		if (!model)
			return;
		s_Data.DepthShader->UploadUniformInt("u_IsTerrain", 0);
		s_Data.DepthShader->UploadUniformMat4("u_Transform", transform);
		for (const Ref<Mesh>& mesh : model->GetMeshes())
		{
			if (mesh && mesh->GetVertexArray() && mesh->GetIndexCount() > 0)
				RenderCommand::DrawIndexed(mesh->GetVertexArray(), mesh->GetIndexCount());
		}
	}

	void ShadowRenderer::SubmitTerrain(TerrainComponent& terrain, const glm::mat4& transform)
	{
		if (!s_Data.PassActive || !terrain.Runtime || !terrain.Runtime->Mesh
			|| !terrain.Runtime->HeightMap)
			return;
		s_Data.DepthShader->UploadUniformInt("u_IsTerrain", 1);
		s_Data.DepthShader->UploadUniformMat4("u_Transform", transform);
		s_Data.DepthShader->UploadUniformFloat(
			"u_MaxHeight", terrain.Specification.HeightScale);
		terrain.Runtime->HeightMap->Bind(0);
		RenderCommand::DrawIndexed(
			terrain.Runtime->Mesh->GetVertexArray(), terrain.Runtime->Mesh->GetIndexCount());
	}

	void ShadowRenderer::EndDirectional()
	{
		if (!s_Data.PassActive)
			return;
		s_Data.DepthShader->Unbind();
		s_Data.Framebuffer->Unbind();
		s_Data.PassActive = false;
		s_Data.Enabled = true;
	}

	void ShadowRenderer::Disable()
	{
		s_Data.PassActive = false;
		s_Data.Enabled = false;
	}

	void ShadowRenderer::BindForLighting(const Ref<Shader>& shader, uint32_t textureSlot)
	{
		if (!shader)
			return;
		shader->UploadUniformInt("u_ShadowEnabled", s_Data.Enabled ? 1 : 0);
		if (!s_Data.Enabled || !s_Data.Framebuffer)
			return;
		shader->UploadUniformMat4("u_LightViewProjection", s_Data.LightViewProjection);
		shader->UploadUniformFloat("u_ShadowBias", s_Data.Bias);
		shader->UploadUniformFloat("u_ShadowTexelSize",
			1.0f / static_cast<float>(s_Data.Resolution));
		shader->BindTexture("u_ShadowMap", textureSlot,
			s_Data.Framebuffer->GetDepthAttachmentRendererID());
	}

	bool ShadowRenderer::IsEnabled()
	{
		return s_Data.Enabled;
	}
}
