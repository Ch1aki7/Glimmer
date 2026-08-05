#include "glpch.h"
#include "Renderer3D.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/MaterialInstance.h"
#include "Glimmer/Renderer/Model.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/Texture.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <vector>

namespace gl {

	namespace {

		struct RenderKey
		{
			uint64_t Shader = 0;
			uint64_t Material = 0;
			uint32_t Texture = 0;
			uintptr_t Mesh = 0;
			uint32_t Entity = 0;

			bool operator<(const RenderKey& other) const
			{
				return std::tie(Shader, Material, Texture, Mesh, Entity)
					< std::tie(other.Shader, other.Material, other.Texture,
						other.Mesh, other.Entity);
			}
		};

		struct RenderItem
		{
			RenderKey Key;
			Ref<Mesh> MeshResource;
			Ref<Shader> ShaderResource;
			Ref<Texture2D> TextureResource;
			MaterialProperties Material;
			glm::mat4 Transform{ 1.0f };
			int EntityID = -1;
			bool HasBaseColorTexture = false;
		};

		struct Renderer3DData
		{
			glm::mat4 ViewProjection{ 1.0f };
			glm::vec3 CameraPosition{ 0.0f };
			Ref<Texture2D> WhiteTexture;
			std::vector<RenderItem> OpaqueQueue;
			Renderer3D::Statistics Stats;
		};

		Renderer3DData s_Data;

	}

	void Renderer3D::Init()
	{
		s_Data.WhiteTexture = Texture2D::Create(1, 1);
		const uint32_t whitePixel = 0xffffffff;
		s_Data.WhiteTexture->SetData(&whitePixel, sizeof(whitePixel));
		s_Data.OpaqueQueue.reserve(1024);
	}

	void Renderer3D::Shutdown()
	{
		s_Data.OpaqueQueue.clear();
		s_Data.WhiteTexture.reset();
	}

	void Renderer3D::BeginScene(
		const glm::mat4& viewProjection,
		const glm::vec3& cameraPosition)
	{
		s_Data.ViewProjection = viewProjection;
		s_Data.CameraPosition = cameraPosition;
		s_Data.OpaqueQueue.clear();
		ResetStats();
	}

	void Renderer3D::SubmitModel(
		const glm::mat4& transform,
		AssetHandle modelHandle,
		AssetHandle materialHandle,
		int entityID,
		const MaterialOverrides* overrides)
	{
		const Ref<Model> model = AssetManager::GetModel(modelHandle);
		const Ref<Material> material = AssetManager::GetMaterial(materialHandle);
		if (!model || !material)
		{
			s_Data.Stats.SkippedModels++;
			return;
		}

		const MaterialInstance instance(
			material, overrides ? *overrides : MaterialOverrides{});
		const AssetHandle shaderHandle = instance.GetShaderHandle();
		const Ref<Shader> shader = AssetManager::GetShader(shaderHandle);
		if (!shader)
		{
			s_Data.Stats.SkippedModels++;
			return;
		}

		const MaterialProperties properties = instance.GetProperties();
		const Ref<Texture2D> materialTexture =
			AssetManager::GetTexture2D(properties.BaseColorTexture);
		s_Data.Stats.SubmittedModels++;
		s_Data.Stats.ImmediateModeShaderBinds++;

		for (const Ref<Mesh>& mesh : model->GetMeshes())
		{
			if (!mesh || !mesh->GetVertexArray() || mesh->GetIndexCount() == 0)
				continue;

			Ref<Texture2D> texture = materialTexture;
			if (!texture)
				texture = mesh->GetTexture();
			const bool hasBaseColorTexture = static_cast<bool>(texture);
			if (!texture)
				texture = s_Data.WhiteTexture;

			RenderItem item;
			item.Key.Shader = static_cast<uint64_t>(shaderHandle);
			item.Key.Material = static_cast<uint64_t>(materialHandle);
			item.Key.Texture = texture->GetRendererID();
			item.Key.Mesh = reinterpret_cast<uintptr_t>(mesh.get());
			item.Key.Entity = static_cast<uint32_t>(entityID);
			item.MeshResource = mesh;
			item.ShaderResource = shader;
			item.TextureResource = texture;
			item.Material = properties;
			item.Transform = transform;
			item.EntityID = entityID;
			item.HasBaseColorTexture = hasBaseColorTexture;
			s_Data.OpaqueQueue.emplace_back(std::move(item));
			s_Data.Stats.SubmittedItems++;
			s_Data.Stats.ImmediateModeTextureBinds++;
		}
	}

	void Renderer3D::EndScene()
	{
		std::sort(s_Data.OpaqueQueue.begin(), s_Data.OpaqueQueue.end(),
			[](const RenderItem& left, const RenderItem& right) {
				return left.Key < right.Key;
			});
		GL_CORE_ASSERT(std::is_sorted(
			s_Data.OpaqueQueue.begin(), s_Data.OpaqueQueue.end(),
			[](const RenderItem& left, const RenderItem& right) {
				return left.Key < right.Key;
			}), "Renderer3D opaque queue sort invariant failed.");

		Ref<Shader> boundShader;
		uint32_t boundTexture = std::numeric_limits<uint32_t>::max();

		for (const RenderItem& item : s_Data.OpaqueQueue)
		{
			if (item.ShaderResource != boundShader)
			{
				item.ShaderResource->ReloadIfChanged();
				item.ShaderResource->Bind();
				item.ShaderResource->UploadUniformMat4(
					"u_ViewProjection", s_Data.ViewProjection);
				item.ShaderResource->UploadUniformFloat3(
					"u_CameraPos", s_Data.CameraPosition);
				item.ShaderResource->UploadUniformInt("u_BaseColorTexture", 0);
				boundShader = item.ShaderResource;
				s_Data.Stats.ShaderBinds++;
			}

			const uint32_t textureID = item.TextureResource->GetRendererID();
			if (textureID != boundTexture)
			{
				item.TextureResource->Bind(0);
				boundTexture = textureID;
				s_Data.Stats.TextureBinds++;
			}

			item.ShaderResource->UploadUniformMat4("u_Transform", item.Transform);
			item.ShaderResource->UploadUniformInt("u_EntityID", item.EntityID);
			item.ShaderResource->UploadUniformFloat4(
				"u_BaseColor", item.Material.BaseColor);
			item.ShaderResource->UploadUniformFloat(
				"u_Metallic", item.Material.Metallic);
			item.ShaderResource->UploadUniformFloat(
				"u_Roughness", item.Material.Roughness);
			item.ShaderResource->UploadUniformFloat(
				"u_TilingFactor", item.Material.TilingFactor);
			item.ShaderResource->UploadUniformInt(
				"u_HasBaseColorTexture", item.HasBaseColorTexture ? 1 : 0);

			RenderCommand::DrawIndexed(
				item.MeshResource->GetVertexArray(),
				item.MeshResource->GetIndexCount());
			s_Data.Stats.DrawCalls++;
		}
		GL_CORE_ASSERT(s_Data.Stats.DrawCalls == s_Data.Stats.SubmittedItems,
			"Renderer3D did not execute every submitted render item.");
		GL_CORE_ASSERT(s_Data.Stats.ShaderBinds
			<= s_Data.Stats.ImmediateModeShaderBinds,
			"Renderer3D shader state cache regressed immediate-mode binds.");
		GL_CORE_ASSERT(s_Data.Stats.TextureBinds
			<= s_Data.Stats.ImmediateModeTextureBinds,
			"Renderer3D texture state cache regressed immediate-mode binds.");

		s_Data.OpaqueQueue.clear();
	}

	void Renderer3D::ResetStats()
	{
		s_Data.Stats = {};
	}

	Renderer3D::Statistics Renderer3D::GetStats()
	{
		return s_Data.Stats;
	}

}
