#include "glpch.h"
#include "Renderer3D.h"
#include "ShadowRenderer.h"

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
		constexpr uint32_t MaterialTextureCount = 4;
		enum MaterialTextureSlot : uint32_t
		{
			BaseColorSlot = 0,
			NormalSlot,
			AOSlot,
			EmissiveSlot
		};

		uint32_t FloatBits(float value)
		{
			uint32_t bits = 0;
			static_assert(sizeof(bits) == sizeof(value), "Unexpected float size.");
			std::memcpy(&bits, &value, sizeof(bits));
			return bits;
		}

		struct MaterialSortKey
		{
			std::array<uint64_t, MaterialTextureCount> Textures{};
			std::array<uint32_t, 15> Values{};

			bool operator<(const MaterialSortKey& other) const
			{
				return std::tie(Textures, Values) < std::tie(other.Textures, other.Values);
			}
		};

		MaterialSortKey MakeMaterialSortKey(const MaterialProperties& material)
		{
			return {
				{ static_cast<uint64_t>(material.BaseColorTexture),
				  static_cast<uint64_t>(material.NormalTexture),
				  static_cast<uint64_t>(material.AOTexture),
				  static_cast<uint64_t>(material.EmissiveTexture) },
				{ FloatBits(material.BaseColor.x), FloatBits(material.BaseColor.y),
				  FloatBits(material.BaseColor.z), FloatBits(material.BaseColor.w),
				  FloatBits(material.TilingFactor), FloatBits(material.Metallic),
				  FloatBits(material.Roughness), FloatBits(material.NormalScale),
				  FloatBits(material.AOStrength), FloatBits(material.EmissiveColor.x),
				  FloatBits(material.EmissiveColor.y), FloatBits(material.EmissiveColor.z),
				  FloatBits(material.EmissiveStrength),
				  static_cast<uint32_t>(material.AlphaMode),
				  FloatBits(material.AlphaCutoff) }
			};
		}

		struct RenderKey
		{
			uint64_t Shader = 0;
			uint64_t Material = 0;
			std::array<uint32_t, MaterialTextureCount> Textures{};
			uintptr_t Mesh = 0;
			MaterialSortKey MaterialState;
			std::array<bool, MaterialTextureCount> HasTextures{};
			uint32_t Entity = 0;

			bool operator<(const RenderKey& other) const
			{
				return std::tie(Shader, Material, Textures, Mesh, MaterialState,
					HasTextures, Entity)
					< std::tie(other.Shader, other.Material, other.Textures,
						other.Mesh, other.MaterialState,
						other.HasTextures, other.Entity);
			}
		};

		struct RenderItem
		{
			RenderKey Key;
			Ref<Mesh> MeshResource;
			Ref<Shader> ShaderResource;
			std::array<Ref<Texture2D>, MaterialTextureCount> TextureResources;
			MaterialProperties Material;
			glm::mat4 Transform{ 1.0f };
			int EntityID = -1;
			std::array<bool, MaterialTextureCount> HasTextures{};
			float CameraDistanceSquared = 0.0f;
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
			std::vector<RenderItem> TransparentQueue;
			std::unordered_map<MaterialCacheKey, MaterialCacheEntry,
				MaterialCacheKeyHash> MaterialCache;
			uint64_t FrameIndex = 0;
			bool SceneActive = false;
			Renderer3D::Statistics Stats;
		};

		Renderer3DData s_Data;

		bool CanBatch(const RenderItem& left, const RenderItem& right)
		{
			return left.ShaderResource == right.ShaderResource
				&& left.TextureResources == right.TextureResources
				&& left.MeshResource == right.MeshResource
				&& left.Material == right.Material
				&& left.HasTextures == right.HasTextures;
		}

		Ref<Texture2D> ResolveMaterialTexture(AssetHandle handle,
			TextureColorSpace colorSpace, TextureSemantic semantic)
		{
			if (static_cast<uint64_t>(handle) == 0)
				return nullptr;
			const AssetMetadata metadata = AssetManager::GetMetadata(handle);
			if (metadata.Type != AssetType::Texture2D)
				return nullptr;
			const bool semanticCompatible = metadata.Semantic == semantic
				|| (semantic == TextureSemantic::Data
					&& metadata.Semantic == TextureSemantic::Height);
			if (metadata.ColorSpace != colorSpace || !semanticCompatible)
				return nullptr;
			return AssetManager::GetTexture2D(handle);
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
			item.ShaderResource->UploadUniformFloat("u_NormalScale", item.Material.NormalScale);
			item.ShaderResource->UploadUniformFloat("u_AOStrength", item.Material.AOStrength);
			item.ShaderResource->UploadUniformFloat3("u_EmissiveColor", item.Material.EmissiveColor);
			item.ShaderResource->UploadUniformFloat(
				"u_EmissiveStrength", item.Material.EmissiveStrength);
			item.ShaderResource->UploadUniformFloat("u_TilingFactor", item.Material.TilingFactor);
			item.ShaderResource->UploadUniformInt(
				"u_HasBaseColorTexture", item.HasTextures[BaseColorSlot] ? 1 : 0);
			item.ShaderResource->UploadUniformInt(
				"u_HasNormalTexture", item.HasTextures[NormalSlot] ? 1 : 0);
			item.ShaderResource->UploadUniformInt(
				"u_HasAOTexture", item.HasTextures[AOSlot] ? 1 : 0);
			item.ShaderResource->UploadUniformInt(
				"u_HasEmissiveTexture", item.HasTextures[EmissiveSlot] ? 1 : 0);
			item.ShaderResource->UploadUniformInt(
				"u_AlphaMode", static_cast<int>(item.Material.AlphaMode));
			item.ShaderResource->UploadUniformFloat(
				"u_AlphaCutoff", item.Material.AlphaCutoff);
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
		s_Data.TransparentQueue.reserve(256);
		s_Data.MaterialCache.reserve(1024);
	}

	void Renderer3D::Shutdown()
	{
		s_Data.OpaqueQueue.clear();
		s_Data.TransparentQueue.clear();
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
		s_Data.TransparentQueue.clear();
		s_Data.SceneActive = true;
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
		std::array<Ref<Texture2D>, MaterialTextureCount> materialTextures{
			ResolveMaterialTexture(properties.BaseColorTexture,
				TextureColorSpace::SRGB, TextureSemantic::Color),
			ResolveMaterialTexture(properties.NormalTexture,
				TextureColorSpace::Linear, TextureSemantic::Normal),
			ResolveMaterialTexture(properties.AOTexture,
				TextureColorSpace::Linear, TextureSemantic::Data),
			ResolveMaterialTexture(properties.EmissiveTexture,
				TextureColorSpace::SRGB, TextureSemantic::Color)
		};
		s_Data.Stats.SubmittedModels++;
		s_Data.Stats.ImmediateModeShaderBinds++;

		for (const Ref<Mesh>& mesh : model->GetMeshes())
		{
			if (!mesh || !mesh->GetVertexArray() || mesh->GetIndexCount() == 0)
				continue;

			std::array<Ref<Texture2D>, MaterialTextureCount> textures = materialTextures;
			if (!textures[BaseColorSlot])
				textures[BaseColorSlot] = mesh->GetTexture();
			std::array<bool, MaterialTextureCount> hasTextures{};
			for (uint32_t slot = 0; slot < MaterialTextureCount; ++slot)
			{
				hasTextures[slot] = static_cast<bool>(textures[slot]);
				if (!textures[slot])
					textures[slot] = s_Data.WhiteTexture;
			}

			RenderItem item;
			item.Key.Shader = static_cast<uint64_t>(shaderHandle);
			item.Key.Material = static_cast<uint64_t>(materialHandle);
			for (uint32_t slot = 0; slot < MaterialTextureCount; ++slot)
				item.Key.Textures[slot] = textures[slot]->GetRendererID();
			item.Key.Mesh = reinterpret_cast<uintptr_t>(mesh.get());
			item.Key.MaterialState = MakeMaterialSortKey(properties);
			item.Key.HasTextures = hasTextures;
			item.Key.Entity = static_cast<uint32_t>(entityID);
			item.MeshResource = mesh;
			item.ShaderResource = shader;
			item.TextureResources = textures;
			item.Material = properties;
			item.Transform = transform;
			item.EntityID = entityID;
			item.HasTextures = hasTextures;
			const glm::vec3 itemPosition = glm::vec3(transform[3]);
			const glm::vec3 cameraOffset = itemPosition - s_Data.CameraPosition;
			item.CameraDistanceSquared = glm::dot(cameraOffset, cameraOffset);
			if (properties.AlphaMode == MaterialAlphaMode::Blend)
			{
				s_Data.TransparentQueue.emplace_back(std::move(item));
				s_Data.Stats.TransparentItems++;
			}
			else
			{
				s_Data.OpaqueQueue.emplace_back(std::move(item));
				if (properties.AlphaMode == MaterialAlphaMode::Mask)
					s_Data.Stats.MaskItems++;
				else
					s_Data.Stats.OpaqueItems++;
			}
			s_Data.Stats.SubmittedItems++;
			s_Data.Stats.ImmediateModeTextureBinds += MaterialTextureCount;
		}
	}

	void Renderer3D::FlushOpaqueAndMask()
	{
		if (!s_Data.SceneActive)
			return;
		RenderCommand::SetBlendEnabled(false);
		RenderCommand::SetDepthWriteEnabled(true);
		RenderCommand::SetDepthFunction(DepthFunction::Less);
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
		std::array<uint32_t, MaterialTextureCount> boundTextures;
		boundTextures.fill(std::numeric_limits<uint32_t>::max());

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
				item.ShaderResource->UploadUniformInt("u_NormalTexture", 1);
				item.ShaderResource->UploadUniformInt("u_AOTexture", 2);
				item.ShaderResource->UploadUniformInt("u_EmissiveTexture", 3);
				ShadowRenderer::BindForLighting(item.ShaderResource, 4);
				boundShader = item.ShaderResource;
				s_Data.Stats.ShaderBinds++;
			}

			for (uint32_t slot = 0; slot < MaterialTextureCount; ++slot)
			{
				const uint32_t textureID = item.TextureResources[slot]->GetRendererID();
				if (textureID != boundTextures[slot])
				{
					item.TextureResources[slot]->Bind(slot);
					boundTextures[slot] = textureID;
					s_Data.Stats.TextureBinds++;
				}
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
		s_Data.OpaqueQueue.clear();
	}

	void Renderer3D::EndScene()
	{
		if (!s_Data.SceneActive)
			return;

		FlushOpaqueAndMask();
		auto transparentOrder = [](const RenderItem& left, const RenderItem& right) {
			if (left.CameraDistanceSquared != right.CameraDistanceSquared)
				return left.CameraDistanceSquared > right.CameraDistanceSquared;
			return left.Key < right.Key;
		};
		std::stable_sort(s_Data.TransparentQueue.begin(),
			s_Data.TransparentQueue.end(), transparentOrder);
		GL_CORE_ASSERT(std::is_sorted(s_Data.TransparentQueue.begin(),
			s_Data.TransparentQueue.end(), transparentOrder),
			"Renderer3D transparent queue sort invariant failed.");

		RenderCommand::SetBlendFunction(
			BlendFactor::SourceAlpha, BlendFactor::OneMinusSourceAlpha);
		RenderCommand::SetBlendEnabled(true);
		RenderCommand::SetDepthWriteEnabled(false);
		RenderCommand::SetDepthFunction(DepthFunction::Less);

		Ref<Shader> boundShader;
		std::array<uint32_t, MaterialTextureCount> boundTextures;
		boundTextures.fill(std::numeric_limits<uint32_t>::max());
		for (const RenderItem& item : s_Data.TransparentQueue)
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
				item.ShaderResource->UploadUniformInt("u_NormalTexture", 1);
				item.ShaderResource->UploadUniformInt("u_AOTexture", 2);
				item.ShaderResource->UploadUniformInt("u_EmissiveTexture", 3);
				ShadowRenderer::BindForLighting(item.ShaderResource, 4);
				boundShader = item.ShaderResource;
				s_Data.Stats.ShaderBinds++;
			}

			for (uint32_t slot = 0; slot < MaterialTextureCount; ++slot)
			{
				const uint32_t textureID = item.TextureResources[slot]->GetRendererID();
				if (textureID != boundTextures[slot])
				{
					item.TextureResources[slot]->Bind(slot);
					boundTextures[slot] = textureID;
					s_Data.Stats.TextureBinds++;
				}
			}

			UploadMaterialState(item);
			if (item.ShaderResource->SupportsInstancing())
				item.ShaderResource->UploadUniformInt("u_UseInstancing", 0);
			item.ShaderResource->UploadUniformMat4("u_Transform", item.Transform);
			item.ShaderResource->UploadUniformInt("u_EntityID", item.EntityID);
			RenderCommand::DrawIndexed(
				item.MeshResource->GetVertexArray(), item.MeshResource->GetIndexCount());
			s_Data.Stats.DrawCalls++;
			s_Data.Stats.IndividualDrawCalls++;
			s_Data.Stats.TransparentDrawCalls++;
			s_Data.Stats.BatchCount++;
			s_Data.Stats.RenderedItems++;
		}

		RenderCommand::SetBlendEnabled(false);
		RenderCommand::SetBlendFunction(
			BlendFactor::SourceAlpha, BlendFactor::OneMinusSourceAlpha);
		RenderCommand::SetDepthWriteEnabled(true);
		RenderCommand::SetDepthFunction(DepthFunction::Less);

		GL_CORE_ASSERT(s_Data.Stats.RenderedItems == s_Data.Stats.SubmittedItems,
			"Renderer3D did not execute every submitted render item.");
		GL_CORE_ASSERT(s_Data.Stats.ShaderBinds
			<= s_Data.Stats.ImmediateModeShaderBinds,
			"Renderer3D shader state cache regressed immediate-mode binds.");
		GL_CORE_ASSERT(s_Data.Stats.TextureBinds
			<= s_Data.Stats.ImmediateModeTextureBinds,
			"Renderer3D texture state cache regressed immediate-mode binds.");

		s_Data.TransparentQueue.clear();
		for (auto iterator = s_Data.MaterialCache.begin();
			iterator != s_Data.MaterialCache.end();)
		{
			if (s_Data.FrameIndex > iterator->second.LastUsedFrame + 120)
				iterator = s_Data.MaterialCache.erase(iterator);
			else
				++iterator;
		}
		s_Data.SceneActive = false;
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
