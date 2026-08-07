#include "glpch.h"
#include "Renderer3D.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/MaterialInstance.h"
#include "Glimmer/Renderer/Model.h"
#include "Glimmer/Renderer/Buffer.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/Texture.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace gl {

	namespace {
		constexpr uint32_t MaxInstancesPerDraw = 1024;

		uint32_t FloatBits(float value)
		{
			uint32_t bits = 0;
			static_assert(sizeof(bits) == sizeof(value), "Unexpected float size.");
			std::memcpy(&bits, &value, sizeof(bits));
			return bits;
		}

		struct MaterialSortKey
		{
			uint64_t BaseColorTexture = 0;
			std::array<uint32_t, 7> Values{};

			bool operator<(const MaterialSortKey& other) const
			{
				return std::tie(BaseColorTexture, Values)
					< std::tie(other.BaseColorTexture, other.Values);
			}
		};

		MaterialSortKey MakeMaterialSortKey(const MaterialProperties& material)
		{
			return {
				static_cast<uint64_t>(material.BaseColorTexture),
				{ FloatBits(material.BaseColor.x), FloatBits(material.BaseColor.y),
				  FloatBits(material.BaseColor.z), FloatBits(material.BaseColor.w),
				  FloatBits(material.TilingFactor), FloatBits(material.Metallic),
				  FloatBits(material.Roughness) }
			};
		}

		struct RenderKey
		{
			uint64_t Shader = 0;
			uint64_t Material = 0;
			uint32_t Texture = 0;
			uintptr_t Mesh = 0;
			MaterialSortKey MaterialState;
			bool HasBaseColorTexture = false;
			uint32_t Entity = 0;

			bool operator<(const RenderKey& other) const
			{
				return std::tie(Shader, Material, Texture, Mesh, MaterialState,
					HasBaseColorTexture, Entity)
					< std::tie(other.Shader, other.Material, other.Texture,
						other.Mesh, other.MaterialState,
						other.HasBaseColorTexture, other.Entity);
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

		struct InstanceData
		{
			glm::mat4 Transform{ 1.0f };
			glm::ivec4 EntityData{ -1, 0, 0, 0 };
		};
		static_assert(offsetof(InstanceData, EntityData) == sizeof(glm::mat4),
			"InstanceData must match the declared BufferLayout.");
		static_assert(sizeof(InstanceData) == sizeof(glm::mat4) + sizeof(glm::ivec4),
			"InstanceData contains unexpected padding.");

		struct MaterialCacheKey
		{
			int EntityID = -1;
			uint64_t MaterialHandle = 0;

			bool operator==(const MaterialCacheKey& other) const
			{
				return EntityID == other.EntityID
					&& MaterialHandle == other.MaterialHandle;
			}
		};

		struct MaterialCacheKeyHash
		{
			size_t operator()(const MaterialCacheKey& key) const
			{
				return std::hash<uint64_t>{}(
					(static_cast<uint64_t>(static_cast<uint32_t>(key.EntityID)) << 32)
					^ key.MaterialHandle);
			}
		};

		struct MaterialCacheEntry
		{
			uint64_t MaterialVersion = 0;
			uint64_t OverrideVersion = 0;
			MaterialState BaseState;
			MaterialOverrides Overrides;
			AssetHandle ShaderHandle{ 0 };
			MaterialProperties Properties;
			uint64_t LastUsedFrame = 0;
		};

		struct Renderer3DData
		{
			glm::mat4 ViewProjection{ 1.0f };
			glm::vec3 CameraPosition{ 0.0f };
			Ref<Texture2D> WhiteTexture;
			Ref<VertexBuffer> InstanceVertexBuffer;
			std::vector<InstanceData> InstanceBuffer;
			std::vector<RenderItem> OpaqueQueue;
			std::unordered_map<MaterialCacheKey, MaterialCacheEntry,
				MaterialCacheKeyHash> MaterialCache;
			uint64_t FrameIndex = 0;
			Renderer3D::Statistics Stats;
		};

		Renderer3DData s_Data;

		bool CanBatch(const RenderItem& left, const RenderItem& right)
		{
			return left.ShaderResource == right.ShaderResource
				&& left.TextureResource == right.TextureResource
				&& left.MeshResource == right.MeshResource
				&& left.Material == right.Material
				&& left.HasBaseColorTexture == right.HasBaseColorTexture;
		}

		void EnsureInstanceInput(const Ref<VertexArray>& vertexArray)
		{
			const auto& buffers = vertexArray->GetVertexBuffers();
			if (std::find(buffers.begin(), buffers.end(), s_Data.InstanceVertexBuffer)
				== buffers.end())
				vertexArray->AddVertexBuffer(s_Data.InstanceVertexBuffer);
		}

		void UploadMaterialState(const RenderItem& item)
		{
			item.ShaderResource->UploadUniformFloat4("u_BaseColor", item.Material.BaseColor);
			item.ShaderResource->UploadUniformFloat("u_Metallic", item.Material.Metallic);
			item.ShaderResource->UploadUniformFloat("u_Roughness", item.Material.Roughness);
			item.ShaderResource->UploadUniformFloat("u_TilingFactor", item.Material.TilingFactor);
			item.ShaderResource->UploadUniformInt(
				"u_HasBaseColorTexture", item.HasBaseColorTexture ? 1 : 0);
		}

	}

	void Renderer3D::Init()
	{
		s_Data.WhiteTexture = Texture2D::Create(1, 1);
		const uint32_t whitePixel = 0xffffffff;
		s_Data.WhiteTexture->SetData(&whitePixel, sizeof(whitePixel));
		s_Data.InstanceVertexBuffer = VertexBuffer::Create(
			MaxInstancesPerDraw * static_cast<uint32_t>(sizeof(InstanceData)));
		s_Data.InstanceVertexBuffer->SetLayout({
			{ ShaderDataType::Mat4, "a_InstanceTransform", false, BufferInputRate::PerInstance },
			{ ShaderDataType::Int4, "a_InstanceEntityData", false, BufferInputRate::PerInstance }
		});
		s_Data.InstanceBuffer.reserve(MaxInstancesPerDraw);
		s_Data.OpaqueQueue.reserve(1024);
		s_Data.MaterialCache.reserve(1024);
	}

	void Renderer3D::Shutdown()
	{
		s_Data.OpaqueQueue.clear();
		s_Data.InstanceBuffer.clear();
		s_Data.MaterialCache.clear();
		s_Data.InstanceVertexBuffer.reset();
		s_Data.WhiteTexture.reset();
	}

	void Renderer3D::BeginScene(
		const glm::mat4& viewProjection,
		const glm::vec3& cameraPosition)
	{
		s_Data.ViewProjection = viewProjection;
		s_Data.CameraPosition = cameraPosition;
		s_Data.OpaqueQueue.clear();
		++s_Data.FrameIndex;
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

		const MaterialOverrides resolvedOverrides = overrides
			? *overrides : MaterialOverrides{};
		const MaterialCacheKey cacheKey{
			entityID, static_cast<uint64_t>(materialHandle) };
		auto [cacheIterator, inserted] = s_Data.MaterialCache.try_emplace(cacheKey);
		MaterialCacheEntry& cached = cacheIterator->second;
		const MaterialState baseState = material->GetState();
		if (inserted || cached.BaseState != baseState
			|| cached.Overrides != resolvedOverrides)
		{
			const MaterialInstance instance(material, resolvedOverrides);
			cached.ShaderHandle = instance.GetShaderHandle();
			cached.Properties = instance.GetProperties();
			cached.BaseState = baseState;
			cached.Overrides = resolvedOverrides;
			cached.MaterialVersion = material->GetVersion();
			cached.OverrideVersion = resolvedOverrides.GetVersion();
			s_Data.Stats.MaterialCacheMisses++;
		}
		else
		{
			cached.MaterialVersion = material->GetVersion();
			cached.OverrideVersion = resolvedOverrides.GetVersion();
			s_Data.Stats.MaterialCacheHits++;
		}
		cached.LastUsedFrame = s_Data.FrameIndex;

		const AssetHandle shaderHandle = cached.ShaderHandle;
		const Ref<Shader> shader = AssetManager::GetShader(shaderHandle);
		if (!shader)
		{
			s_Data.Stats.SkippedModels++;
			return;
		}

		const MaterialProperties properties = cached.Properties;
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
			item.Key.MaterialState = MakeMaterialSortKey(properties);
			item.Key.HasBaseColorTexture = hasBaseColorTexture;
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

		for (size_t itemIndex = 0; itemIndex < s_Data.OpaqueQueue.size();)
		{
			const RenderItem& item = s_Data.OpaqueQueue[itemIndex];
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

			size_t batchEnd = itemIndex + 1;
			while (batchEnd < s_Data.OpaqueQueue.size()
				&& CanBatch(item, s_Data.OpaqueQueue[batchEnd]))
				batchEnd++;

			UploadMaterialState(item);
			const size_t batchSize = batchEnd - itemIndex;
			s_Data.Stats.BatchCount++;
			if (batchSize > 1 && item.ShaderResource->SupportsInstancing())
			{
				EnsureInstanceInput(item.MeshResource->GetVertexArray());
				item.ShaderResource->UploadUniformInt("u_UseInstancing", 1);
				for (size_t chunkBegin = itemIndex; chunkBegin < batchEnd;)
				{
					const size_t chunkEnd = std::min(
						chunkBegin + static_cast<size_t>(MaxInstancesPerDraw), batchEnd);
					s_Data.InstanceBuffer.clear();
					for (size_t index = chunkBegin; index < chunkEnd; index++)
					{
						const RenderItem& instance = s_Data.OpaqueQueue[index];
						s_Data.InstanceBuffer.push_back({
							instance.Transform, glm::ivec4(instance.EntityID, 0, 0, 0) });
					}
					s_Data.InstanceVertexBuffer->SetData(
						s_Data.InstanceBuffer.data(),
						static_cast<uint32_t>(s_Data.InstanceBuffer.size() * sizeof(InstanceData)));
					RenderCommand::DrawIndexedInstanced(
						item.MeshResource->GetVertexArray(),
						static_cast<uint32_t>(s_Data.InstanceBuffer.size()),
						item.MeshResource->GetIndexCount());
					s_Data.Stats.DrawCalls++;
					s_Data.Stats.InstancedDrawCalls++;
					s_Data.Stats.InstanceCount +=
						static_cast<uint32_t>(s_Data.InstanceBuffer.size());
					s_Data.Stats.RenderedItems +=
						static_cast<uint32_t>(s_Data.InstanceBuffer.size());
					chunkBegin = chunkEnd;
				}
			}
			else
			{
				if (item.ShaderResource->SupportsInstancing())
					item.ShaderResource->UploadUniformInt("u_UseInstancing", 0);
				for (size_t index = itemIndex; index < batchEnd; index++)
				{
					const RenderItem& individual = s_Data.OpaqueQueue[index];
					individual.ShaderResource->UploadUniformMat4(
						"u_Transform", individual.Transform);
					individual.ShaderResource->UploadUniformInt(
						"u_EntityID", individual.EntityID);
					RenderCommand::DrawIndexed(
						individual.MeshResource->GetVertexArray(),
						individual.MeshResource->GetIndexCount());
					s_Data.Stats.DrawCalls++;
					s_Data.Stats.IndividualDrawCalls++;
					s_Data.Stats.RenderedItems++;
				}
			}
			itemIndex = batchEnd;
		}
		GL_CORE_ASSERT(s_Data.Stats.RenderedItems == s_Data.Stats.SubmittedItems,
			"Renderer3D did not execute every submitted render item.");
		GL_CORE_ASSERT(s_Data.Stats.ShaderBinds
			<= s_Data.Stats.ImmediateModeShaderBinds,
			"Renderer3D shader state cache regressed immediate-mode binds.");
		GL_CORE_ASSERT(s_Data.Stats.TextureBinds
			<= s_Data.Stats.ImmediateModeTextureBinds,
			"Renderer3D texture state cache regressed immediate-mode binds.");
		s_Data.OpaqueQueue.clear();
		for (auto iterator = s_Data.MaterialCache.begin();
			iterator != s_Data.MaterialCache.end();)
		{
			if (s_Data.FrameIndex > iterator->second.LastUsedFrame + 120)
				iterator = s_Data.MaterialCache.erase(iterator);
			else
				++iterator;
		}
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
