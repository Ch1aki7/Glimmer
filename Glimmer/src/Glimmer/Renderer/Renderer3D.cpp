#include "glpch.h"
#include "Renderer3D.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/Material.h"
#include "Glimmer/Renderer/Model.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/Texture.h"

namespace gl {

	namespace {

		struct Renderer3DData
		{
			glm::mat4 ViewProjection{ 1.0f };
			glm::vec3 CameraPosition{ 0.0f };
			Ref<Texture2D> WhiteTexture;
		};

		Renderer3DData s_Data;

	}

	void Renderer3D::Init()
	{
		s_Data.WhiteTexture = Texture2D::Create(1, 1);
		const uint32_t whitePixel = 0xffffffff;
		s_Data.WhiteTexture->SetData(&whitePixel, sizeof(whitePixel));
	}

	void Renderer3D::Shutdown()
	{
		s_Data.WhiteTexture.reset();
	}

	void Renderer3D::BeginScene(
		const glm::mat4& viewProjection,
		const glm::vec3& cameraPosition)
	{
		s_Data.ViewProjection = viewProjection;
		s_Data.CameraPosition = cameraPosition;
	}

	void Renderer3D::DrawModel(
		const glm::mat4& transform,
		AssetHandle modelHandle,
		AssetHandle materialHandle,
		int entityID)
	{
		const Ref<Model> model = AssetManager::GetModel(modelHandle);
		const Ref<Material> material = AssetManager::GetMaterial(materialHandle);
		if (!model || !material)
			return;

		const Ref<Shader> shader = AssetManager::GetShader(material->GetShaderHandle());
		if (!shader)
			return;

		shader->ReloadIfChanged();
		shader->Bind();
		shader->UploadUniformMat4("u_ViewProjection", s_Data.ViewProjection);
		shader->UploadUniformMat4("u_Transform", transform);
		shader->UploadUniformFloat3("u_CameraPos", s_Data.CameraPosition);
		shader->UploadUniformInt("u_EntityID", entityID);

		const auto& properties = material->GetProperties();
		shader->UploadUniformFloat4("u_BaseColor", properties.BaseColor);
		shader->UploadUniformFloat("u_Metallic", properties.Metallic);
		shader->UploadUniformFloat("u_Roughness", properties.Roughness);
		shader->UploadUniformFloat("u_TilingFactor", properties.TilingFactor);

		const Ref<Texture2D> materialTexture =
			AssetManager::GetTexture2D(properties.BaseColorTexture);

		for (const auto& mesh : model->GetMeshes())
		{
			Ref<Texture2D> texture = materialTexture;
			if (!texture)
				texture = mesh->GetTexture();
			if (!texture)
				texture = s_Data.WhiteTexture;

			texture->Bind(0);
			shader->UploadUniformInt("u_BaseColorTexture", 0);
			shader->UploadUniformInt("u_HasBaseColorTexture",
				texture != s_Data.WhiteTexture ? 1 : 0);

			mesh->Bind();
			RenderCommand::DrawIndexed(mesh->GetVertexArray(), mesh->GetIndexCount());
		}
	}

}