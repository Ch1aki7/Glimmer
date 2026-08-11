#include "glpch.h"
#include "ShadowRenderer.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Renderer/Buffer.h"
#include "Glimmer/Renderer/FrameBuffer.h"
#include "Glimmer/Renderer/GPUTimer.h"
#include "Glimmer/Renderer/MaterialInstance.h"
#include "Glimmer/Renderer/Mesh.h"
#include "Glimmer/Renderer/Model.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/Texture.h"
#include "Glimmer/Renderer/VertexArray.h"
#include "Glimmer/Scene/Components.h"
#include "Glimmer/Terrain/Terrain.h"

#include <array>
#include <algorithm>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
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

		struct ShadowRenderKey
		{
			uintptr_t Mesh = 0;
			uint32_t Texture = 0;
			std::array<uint32_t, 4> MaterialState{};

			bool operator<(const ShadowRenderKey& other) const
			{
				return std::tie(Mesh, Texture, MaterialState)
					< std::tie(other.Mesh, other.Texture, other.MaterialState);
			}
			bool operator==(const ShadowRenderKey& other) const
			{
				return Mesh == other.Mesh && Texture == other.Texture
					&& MaterialState == other.MaterialState;
			}
		};

		struct ShadowRenderItem
		{
			ShadowRenderKey Key;
			Ref<Mesh> MeshResource;
			Ref<Texture2D> BaseColorTexture;
			glm::mat4 Transform{ 1.0f };
			bool AlphaMasked = false;
			float BaseColorAlpha = 1.0f;
			float AlphaCutoff = 0.5f;
			float TilingFactor = 1.0f;
		};

		struct ShadowVertexArrayCacheEntry
		{
			std::weak_ptr<VertexArray> Source;
			Ref<VertexArray> ShadowVertexArray;
		};

		struct ShadowRendererData
		{
			std::array<Ref<Framebuffer>, ShadowRenderer::MaxCascades> Framebuffers;
			std::array<glm::mat4, ShadowRenderer::MaxCascades> LightViewProjections{};
			std::array<float, ShadowRenderer::MaxCascades> CascadeSplits{};
			std::array<float, ShadowRenderer::MaxCascades> CascadeBlendWidths{};
			Ref<Shader> DepthShader;
			Ref<GPUTimer> Timer;
			Ref<VertexBuffer> InstanceVertexBuffer;
			std::vector<glm::mat4> InstanceBuffer;
			std::vector<ShadowRenderItem> ModelQueue;
			std::unordered_map<const VertexArray*, ShadowVertexArrayCacheEntry>
				ShadowVertexArrays;
			glm::mat4 CameraView{ 1.0f };
			uint32_t Resolution = 0;
			uint32_t CascadeCount = 0;
			uint32_t ActiveCascade = 0;
			float Bias = 0.0015f;
			float LastGpuMilliseconds = 0.0f;
			bool HasGpuTiming = false;
			uint64_t GpuTimingSample = 0;
			ShadowRenderer::Statistics Stats;
			bool PassActive = false;
			bool Enabled = false;
			bool CascadeDebugVisualization = false;
		};

		ShadowRendererData s_Data;

		void EnsureInstancingResources()
		{
			if (s_Data.InstanceVertexBuffer)
				return;
			s_Data.InstanceVertexBuffer = VertexBuffer::Create(
				MaxInstancesPerDraw * static_cast<uint32_t>(sizeof(glm::mat4)));
			s_Data.InstanceVertexBuffer->SetLayout({
				{ ShaderDataType::Mat4, "a_InstanceTransform", false,
					BufferInputRate::PerInstance }
			});
			s_Data.InstanceBuffer.reserve(MaxInstancesPerDraw);
			s_Data.ModelQueue.reserve(1024);
		}

		Ref<VertexArray> GetShadowVertexArray(const Ref<Mesh>& mesh)
		{
			const Ref<VertexArray>& source = mesh->GetVertexArray();
			if (!source || !source->GetIndexBuffer())
				return nullptr;
			auto iterator = s_Data.ShadowVertexArrays.find(source.get());
			if (iterator != s_Data.ShadowVertexArrays.end()
				&& iterator->second.Source.lock() == source
				&& iterator->second.ShadowVertexArray)
				return iterator->second.ShadowVertexArray;

			Ref<VertexArray> shadowVertexArray = VertexArray::Create();
			for (const Ref<VertexBuffer>& vertexBuffer : source->GetVertexBuffers())
			{
				const auto& elements = vertexBuffer->GetLayout().GetElements();
				const bool perVertex = !elements.empty() && std::all_of(
					elements.begin(), elements.end(), [](const BufferElement& element) {
						return element.InputRate == BufferInputRate::PerVertex;
					});
				if (perVertex)
					shadowVertexArray->AddVertexBuffer(vertexBuffer);
			}
			shadowVertexArray->AddVertexBuffer(s_Data.InstanceVertexBuffer);
			shadowVertexArray->SetIndexBuffer(source->GetIndexBuffer());
			s_Data.ShadowVertexArrays[source.get()] = {
				source, shadowVertexArray };
			return shadowVertexArray;
		}

		Ref<Texture2D> ResolveBaseColorTexture(AssetHandle handle)
		{
			if (static_cast<uint64_t>(handle) == 0
				|| !AssetManager::IsAssetHandleValid(handle))
				return nullptr;
			const AssetMetadata metadata = AssetManager::GetMetadata(handle);
			if (metadata.Type != AssetType::Texture2D
				|| metadata.ColorSpace != TextureColorSpace::SRGB
				|| metadata.Semantic != TextureSemantic::Color)
				return nullptr;
			return AssetManager::GetTexture2D(handle);
		}

		void UploadShadowMaterialState(const ShadowRenderItem& item)
		{
			s_Data.DepthShader->UploadUniformInt(
				"u_AlphaMaskEnabled", item.AlphaMasked ? 1 : 0);
			if (!item.AlphaMasked)
				return;
			s_Data.DepthShader->UploadUniformInt(
				"u_HasBaseColorTexture", item.BaseColorTexture ? 1 : 0);
			s_Data.DepthShader->UploadUniformFloat(
				"u_BaseColorAlpha", item.BaseColorAlpha);
			s_Data.DepthShader->UploadUniformFloat(
				"u_AlphaCutoff", item.AlphaCutoff);
			s_Data.DepthShader->UploadUniformFloat(
				"u_TilingFactor", item.TilingFactor);
			if (item.BaseColorTexture)
				item.BaseColorTexture->Bind(0);
		}

		void FlushModelQueue()
		{
			if (s_Data.ModelQueue.empty())
				return;
			std::sort(s_Data.ModelQueue.begin(), s_Data.ModelQueue.end(),
				[](const ShadowRenderItem& left, const ShadowRenderItem& right) {
					return left.Key < right.Key;
				});
			s_Data.DepthShader->UploadUniformInt("u_IsTerrain", 0);

			for (size_t itemIndex = 0; itemIndex < s_Data.ModelQueue.size();)
			{
				const ShadowRenderItem& item = s_Data.ModelQueue[itemIndex];
				size_t batchEnd = itemIndex + 1;
				while (batchEnd < s_Data.ModelQueue.size()
					&& s_Data.ModelQueue[batchEnd].Key == item.Key)
					batchEnd++;

				UploadShadowMaterialState(item);
				const size_t batchSize = batchEnd - itemIndex;
				if (batchSize > 1)
				{
					const Ref<VertexArray> shadowVertexArray =
						GetShadowVertexArray(item.MeshResource);
					if (shadowVertexArray)
					{
						s_Data.DepthShader->UploadUniformInt("u_UseInstancing", 1);
						for (size_t chunkBegin = itemIndex; chunkBegin < batchEnd;)
						{
							const size_t chunkEnd = std::min(
								chunkBegin + static_cast<size_t>(MaxInstancesPerDraw),
								batchEnd);
							s_Data.InstanceBuffer.clear();
							for (size_t index = chunkBegin; index < chunkEnd; ++index)
								s_Data.InstanceBuffer.push_back(
									s_Data.ModelQueue[index].Transform);
							s_Data.InstanceVertexBuffer->SetData(
								s_Data.InstanceBuffer.data(),
								static_cast<uint32_t>(s_Data.InstanceBuffer.size()
									* sizeof(glm::mat4)));
							RenderCommand::DrawIndexedInstanced(
								shadowVertexArray,
								static_cast<uint32_t>(s_Data.InstanceBuffer.size()),
								item.MeshResource->GetIndexCount());
							s_Data.Stats.DrawCalls++;
							s_Data.Stats.InstancedDrawCalls++;
							s_Data.Stats.InstanceCount +=
								static_cast<uint32_t>(s_Data.InstanceBuffer.size());
							chunkBegin = chunkEnd;
						}
						itemIndex = batchEnd;
						continue;
					}
				}

				s_Data.DepthShader->UploadUniformInt("u_UseInstancing", 0);
				for (size_t index = itemIndex; index < batchEnd; ++index)
				{
					const ShadowRenderItem& individual = s_Data.ModelQueue[index];
					s_Data.DepthShader->UploadUniformMat4(
						"u_Transform", individual.Transform);
					RenderCommand::DrawIndexed(
						individual.MeshResource->GetVertexArray(),
						individual.MeshResource->GetIndexCount());
					s_Data.Stats.DrawCalls++;
					s_Data.Stats.IndividualDrawCalls++;
				}
				itemIndex = batchEnd;
			}
			s_Data.ModelQueue.clear();
		}

		std::array<glm::vec3, 8> GetFrustumCornersWorldSpace(
			const glm::mat4& viewProjection)
		{
			const glm::mat4 inverseViewProjection = glm::inverse(viewProjection);
			std::array<glm::vec3, 8> corners{};
			uint32_t index = 0;
			for (uint32_t z = 0; z < 2; ++z)
				for (uint32_t y = 0; y < 2; ++y)
					for (uint32_t x = 0; x < 2; ++x)
					{
						const glm::vec4 clip(
							x ? 1.0f : -1.0f,
							y ? 1.0f : -1.0f,
							z ? 1.0f : -1.0f,
							1.0f);
						const glm::vec4 world = inverseViewProjection * clip;
						corners[index++] = glm::vec3(world) / world.w;
					}
			return corners;
		}

		glm::mat4 BuildCascadeMatrix(
			const std::array<glm::vec3, 8>& fullFrustumCorners,
			float previousSplitRatio,
			float splitRatio,
			const glm::vec3& lightDirection,
			uint32_t resolution)
		{
			std::array<glm::vec3, 8> cascadeCorners{};
			for (uint32_t index = 0; index < 4; ++index)
			{
				const glm::vec3 nearCorner = fullFrustumCorners[index];
				const glm::vec3 farCorner = fullFrustumCorners[index + 4];
				const glm::vec3 range = farCorner - nearCorner;
				cascadeCorners[index] = nearCorner + range * previousSplitRatio;
				cascadeCorners[index + 4] = nearCorner + range * splitRatio;
			}

			glm::vec3 center(0.0f);
			for (const glm::vec3& corner : cascadeCorners)
				center += corner;
			center /= static_cast<float>(cascadeCorners.size());

			float radius = 0.0f;
			for (const glm::vec3& corner : cascadeCorners)
				radius = std::max(radius, glm::length(corner - center));
			radius = std::ceil(radius * 16.0f) / 16.0f;
			radius = std::max(radius, 1.0f);

			const glm::vec3 up = std::abs(glm::dot(
				lightDirection, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.98f
				? glm::vec3(0.0f, 0.0f, 1.0f)
				: glm::vec3(0.0f, 1.0f, 0.0f);
			const glm::mat4 lightView = glm::lookAt(
				center - lightDirection * radius * 2.0f, center, up);
			glm::mat4 lightProjection = glm::ortho(
				-radius, radius, -radius, radius, 0.1f, radius * 4.0f);

			// Snap the projected origin to a shadow texel to reduce shimmering.
			glm::vec4 shadowOrigin = lightProjection * lightView
				* glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			shadowOrigin *= static_cast<float>(resolution) * 0.5f;
			const glm::vec4 roundedOrigin = glm::round(shadowOrigin);
			glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
			roundOffset *= 2.0f / static_cast<float>(resolution);
			lightProjection[3][0] += roundOffset.x;
			lightProjection[3][1] += roundOffset.y;
			return lightProjection * lightView;
		}
	}

	bool ShadowRenderer::IntersectsClipFrustum(
		const glm::vec3& boundsMin,
		const glm::vec3& boundsMax,
		const glm::mat4& transform,
		const glm::mat4& viewProjection)
	{
		bool outsideLeft = true;
		bool outsideRight = true;
		bool outsideBottom = true;
		bool outsideTop = true;
		bool outsideNear = true;
		bool outsideFar = true;
		for (uint32_t z = 0; z < 2; ++z)
			for (uint32_t y = 0; y < 2; ++y)
				for (uint32_t x = 0; x < 2; ++x)
				{
					const glm::vec3 local(
						x ? boundsMax.x : boundsMin.x,
						y ? boundsMax.y : boundsMin.y,
						z ? boundsMax.z : boundsMin.z);
					const glm::vec4 clip = viewProjection * transform
						* glm::vec4(local, 1.0f);
					outsideLeft &= clip.x < -clip.w;
					outsideRight &= clip.x > clip.w;
					outsideBottom &= clip.y < -clip.w;
					outsideTop &= clip.y > clip.w;
					outsideNear &= clip.z < -clip.w;
					outsideFar &= clip.z > clip.w;
				}
		return !(outsideLeft || outsideRight || outsideBottom
			|| outsideTop || outsideNear || outsideFar);
	}

	void ShadowRenderer::Shutdown()
	{
		s_Data.PassActive = false;
		s_Data.Enabled = false;
		s_Data.ModelQueue.clear();
		s_Data.InstanceBuffer.clear();
		s_Data.ShadowVertexArrays.clear();
		s_Data.InstanceVertexBuffer.reset();
		s_Data.Timer.reset();
		s_Data.DepthShader.reset();
		for (Ref<Framebuffer>& framebuffer : s_Data.Framebuffers)
			framebuffer.reset();
		s_Data.Resolution = 0;
		s_Data.CascadeCount = 0;
		s_Data.CascadeDebugVisualization = false;
	}

	bool ShadowRenderer::BeginDirectional(
		const glm::vec3& lightDirection,
		const glm::mat4& cameraView,
		const glm::mat4& cameraProjection,
		float cameraNear,
		float cameraFar,
		uint32_t resolution,
		float distance,
		float bias,
		uint32_t cascadeCount,
		float splitLambda,
		float cascadeBlend)
	{
		Disable();
		s_Data.Stats = {};
		const AssetHandle shaderHandle =
			AssetManager::ImportAsset("assets/shaders/ShadowDepth.glsl");
		s_Data.DepthShader = AssetManager::GetShader(shaderHandle);
		if (!s_Data.DepthShader)
			return false;
		EnsureInstancingResources();
		if (!s_Data.Timer)
			s_Data.Timer = GPUTimer::Create();
		float elapsedMilliseconds = 0.0f;
		if (s_Data.Timer
			&& s_Data.Timer->TryGetElapsedMilliseconds(elapsedMilliseconds))
		{
			s_Data.LastGpuMilliseconds = elapsedMilliseconds;
			s_Data.HasGpuTiming = true;
			++s_Data.GpuTimingSample;
		}
		s_Data.Stats.GpuMilliseconds = s_Data.LastGpuMilliseconds;
		s_Data.Stats.GpuTimingAvailable = s_Data.HasGpuTiming;
		s_Data.Stats.GpuTimingSample = s_Data.GpuTimingSample;

		resolution = std::clamp(resolution, 512u, 4096u);
		cascadeCount = std::clamp(cascadeCount, 1u, MaxCascades);
		if (s_Data.Resolution != resolution)
		{
			for (Ref<Framebuffer>& framebuffer : s_Data.Framebuffers)
				framebuffer.reset();
			s_Data.Resolution = resolution;
		}
		for (uint32_t index = 0; index < cascadeCount; ++index)
		{
			if (s_Data.Framebuffers[index])
				continue;
			FramebufferSpecification specification;
			specification.Width = resolution;
			specification.Height = resolution;
			specification.Attachments = { { FramebufferTextureFormat::Depth32F } };
			s_Data.Framebuffers[index] = Framebuffer::Create(specification);
		}

		const glm::vec3 direction = glm::length(lightDirection) > 0.0001f
			? glm::normalize(lightDirection) : glm::vec3(0.0f, -1.0f, 0.0f);
		cameraNear = std::max(cameraNear, 0.001f);
		cameraFar = std::max(cameraFar, cameraNear + 0.01f);
		const float shadowFar = std::max(cameraNear + 0.01f,
			std::min(cameraFar, std::clamp(distance, 10.0f, 500.0f)));
		const float clipRange = cameraFar - cameraNear;
		const float shadowRange = shadowFar - cameraNear;
		splitLambda = std::clamp(splitLambda, 0.0f, 1.0f);
		cascadeBlend = std::clamp(cascadeBlend, 0.0f, 0.30f);
		const auto frustumCorners = GetFrustumCornersWorldSpace(
			cameraProjection * cameraView);

		std::array<float, MaxCascades + 1> splitDepths{};
		splitDepths[0] = cameraNear;
		for (uint32_t index = 0; index < cascadeCount; ++index)
		{
			const float ratio = static_cast<float>(index + 1)
				/ static_cast<float>(cascadeCount);
			const float logarithmic = cameraNear
				* std::pow(shadowFar / cameraNear, ratio);
			const float uniform = cameraNear + shadowRange * ratio;
			const float splitDepth = glm::mix(uniform, logarithmic, splitLambda);
			splitDepths[index + 1] = splitDepth;
			s_Data.CascadeSplits[index] = splitDepth;
		}

		s_Data.CascadeBlendWidths.fill(0.0f);
		for (uint32_t index = 0; index + 1 < cascadeCount; ++index)
		{
			const float currentRange = splitDepths[index + 1] - splitDepths[index];
			const float nextRange = splitDepths[index + 2] - splitDepths[index + 1];
			s_Data.CascadeBlendWidths[index] =
				std::min(currentRange, nextRange) * cascadeBlend;
		}

		for (uint32_t index = 0; index < cascadeCount; ++index)
		{
			const float nearExtension = index > 0
				? s_Data.CascadeBlendWidths[index - 1] : 0.0f;
			const float farExtension = index + 1 < cascadeCount
				? s_Data.CascadeBlendWidths[index] : 0.0f;
			const float cascadeNear = std::max(
				cameraNear, splitDepths[index] - nearExtension);
			const float cascadeFar = std::min(
				shadowFar, splitDepths[index + 1] + farExtension);
			const float nearRatio = (cascadeNear - cameraNear) / clipRange;
			const float farRatio = (cascadeFar - cameraNear) / clipRange;
			s_Data.LightViewProjections[index] = BuildCascadeMatrix(
				frustumCorners, nearRatio, farRatio, direction, resolution);
		}

		s_Data.CameraView = cameraView;
		s_Data.CascadeCount = cascadeCount;
		s_Data.Bias = std::clamp(bias, 0.00001f, 0.05f);
		s_Data.DepthShader->ReloadIfChanged();
		if (s_Data.Timer)
			s_Data.Timer->Begin();
		return true;
	}

	bool ShadowRenderer::BeginCascade(uint32_t cascadeIndex)
	{
		if (!s_Data.DepthShader || cascadeIndex >= s_Data.CascadeCount
			|| !s_Data.Framebuffers[cascadeIndex])
			return false;
		s_Data.ActiveCascade = cascadeIndex;
		s_Data.ModelQueue.clear();
		s_Data.Framebuffers[cascadeIndex]->Bind();
		RenderCommand::SetBlendEnabled(false);
		RenderCommand::SetDepthWriteEnabled(true);
		RenderCommand::SetDepthFunction(DepthFunction::Less);
		RenderCommand::ClearDepth();
		s_Data.DepthShader->Bind();
		s_Data.DepthShader->UploadUniformMat4("u_LightViewProjection",
			s_Data.LightViewProjections[cascadeIndex]);
		s_Data.DepthShader->UploadUniformInt("u_HeightMap", 0);
		s_Data.DepthShader->UploadUniformInt("u_BaseColorTexture", 0);
		s_Data.DepthShader->UploadUniformInt("u_AlphaMaskEnabled", 0);
		s_Data.DepthShader->UploadUniformInt("u_UseInstancing", 0);
		s_Data.PassActive = true;
		s_Data.Stats.CascadePasses++;
		return true;
	}

	void ShadowRenderer::SubmitModel(
		AssetHandle modelHandle,
		const glm::mat4& transform,
		AssetHandle materialHandle,
		const MaterialOverrides* overrides)
	{
		if (!s_Data.PassActive)
			return;
		const Ref<Model> model = AssetManager::GetModel(modelHandle);
		if (!model)
			return;
		MaterialProperties materialProperties;
		if (static_cast<uint64_t>(materialHandle) != 0)
		{
			const Ref<Material> material = AssetManager::GetMaterial(materialHandle);
			if (material)
			{
				const MaterialInstance instance(
					material, overrides ? *overrides : MaterialOverrides{});
				materialProperties = instance.GetProperties();
			}
		}
		if (!ShouldCastShadow(materialProperties.AlphaMode))
			return;
		const bool alphaMasked =
			materialProperties.AlphaMode == MaterialAlphaMode::Mask;
		const Ref<Texture2D> materialTexture =
			ResolveBaseColorTexture(materialProperties.BaseColorTexture);

		for (const Ref<Mesh>& mesh : model->GetMeshes())
		{
			if (!mesh || !mesh->GetVertexArray() || mesh->GetIndexCount() == 0)
				continue;
			s_Data.Stats.CandidateDraws++;
			if (mesh->HasBounds() && !IntersectsClipFrustum(
				mesh->GetBoundsMin(), mesh->GetBoundsMax(), transform,
				s_Data.LightViewProjections[s_Data.ActiveCascade]))
			{
				s_Data.Stats.CulledDraws++;
				continue;
			}
			const Ref<Texture2D> baseColorTexture = alphaMasked
				? (materialTexture ? materialTexture : mesh->GetTexture()) : nullptr;
			ShadowRenderItem item;
			item.MeshResource = mesh;
			item.BaseColorTexture = baseColorTexture;
			item.Transform = transform;
			item.AlphaMasked = alphaMasked;
			item.BaseColorAlpha = materialProperties.BaseColor.a;
			item.AlphaCutoff = materialProperties.AlphaCutoff;
			item.TilingFactor = materialProperties.TilingFactor;
			item.Key.Mesh = reinterpret_cast<uintptr_t>(mesh.get());
			if (alphaMasked)
			{
				item.Key.Texture = baseColorTexture
					? baseColorTexture->GetRendererID() : 0;
				item.Key.MaterialState = {
					1u,
					FloatBits(item.BaseColorAlpha),
					FloatBits(item.AlphaCutoff),
					FloatBits(item.TilingFactor)
				};
			}
			s_Data.ModelQueue.emplace_back(std::move(item));
			s_Data.Stats.RenderedDraws++;
		}
	}

	void ShadowRenderer::SubmitTerrain(TerrainComponent& terrain, const glm::mat4& transform)
	{
		if (!s_Data.PassActive || !terrain.Runtime || !terrain.Runtime->Mesh
			|| !terrain.Runtime->HeightMap)
			return;
		s_Data.Stats.CandidateDraws++;
		const float halfSize = static_cast<float>(
			terrain.Runtime->Mesh->GetGridSize()) * 0.5f;
		const float minimumHeight = std::min(0.0f, terrain.Specification.HeightScale);
		const float maximumHeight = std::max(0.0f, terrain.Specification.HeightScale);
		if (!IntersectsClipFrustum(
			{ -halfSize, minimumHeight, -halfSize },
			{ halfSize, maximumHeight, halfSize },
			transform, s_Data.LightViewProjections[s_Data.ActiveCascade]))
		{
			s_Data.Stats.CulledDraws++;
			return;
		}
		s_Data.DepthShader->UploadUniformInt("u_IsTerrain", 1);
		s_Data.DepthShader->UploadUniformInt("u_AlphaMaskEnabled", 0);
		s_Data.DepthShader->UploadUniformInt("u_UseInstancing", 0);
		s_Data.DepthShader->UploadUniformMat4("u_Transform", transform);
		s_Data.DepthShader->UploadUniformFloat(
			"u_MaxHeight", terrain.Specification.HeightScale);
		terrain.Runtime->HeightMap->Bind(0);
		RenderCommand::DrawIndexed(
			terrain.Runtime->Mesh->GetVertexArray(), terrain.Runtime->Mesh->GetIndexCount());
		s_Data.Stats.RenderedDraws++;
		s_Data.Stats.DrawCalls++;
		s_Data.Stats.IndividualDrawCalls++;
	}

	void ShadowRenderer::EndCascade()
	{
		if (!s_Data.PassActive)
			return;
		FlushModelQueue();
		s_Data.DepthShader->Unbind();
		s_Data.Framebuffers[s_Data.ActiveCascade]->Unbind();
		s_Data.PassActive = false;
	}

	void ShadowRenderer::EndDirectional()
	{
		if (s_Data.PassActive)
			EndCascade();
		if (s_Data.Timer)
		{
			s_Data.Timer->End();
			float elapsedMilliseconds = 0.0f;
			if (s_Data.Timer->TryGetElapsedMilliseconds(elapsedMilliseconds))
			{
				s_Data.LastGpuMilliseconds = elapsedMilliseconds;
				s_Data.HasGpuTiming = true;
				++s_Data.GpuTimingSample;
				s_Data.Stats.GpuMilliseconds = elapsedMilliseconds;
				s_Data.Stats.GpuTimingAvailable = true;
				s_Data.Stats.GpuTimingSample = s_Data.GpuTimingSample;
			}
		}
		s_Data.Enabled = s_Data.CascadeCount > 0;
	}

	void ShadowRenderer::Disable()
	{
		s_Data.PassActive = false;
		s_Data.Enabled = false;
		s_Data.CascadeCount = 0;
		s_Data.ModelQueue.clear();
		s_Data.Stats = {};
	}

	void ShadowRenderer::BindForLighting(const Ref<Shader>& shader, uint32_t textureSlot)
	{
		if (!shader)
			return;
		shader->UploadUniformInt("u_ShadowEnabled", s_Data.Enabled ? 1 : 0);
		shader->UploadUniformInt("u_ShadowCascadeDebug",
			s_Data.Enabled && s_Data.CascadeDebugVisualization ? 1 : 0);
		if (!s_Data.Enabled)
			return;
		shader->UploadUniformInt("u_ShadowCascadeCount",
			static_cast<int>(s_Data.CascadeCount));
		shader->UploadUniformMat4("u_ShadowCameraView", s_Data.CameraView);
		shader->UploadUniformFloat("u_ShadowBias", s_Data.Bias);
		shader->UploadUniformFloat("u_ShadowTexelSize",
			1.0f / static_cast<float>(s_Data.Resolution));
		for (uint32_t index = 0; index < s_Data.CascadeCount; ++index)
		{
			shader->UploadUniformMat4(
				"u_LightViewProjections[" + std::to_string(index) + "]",
				s_Data.LightViewProjections[index]);
			shader->UploadUniformFloat(
				"u_ShadowCascadeSplits[" + std::to_string(index) + "]",
				s_Data.CascadeSplits[index]);
			shader->UploadUniformFloat(
				"u_ShadowCascadeBlendWidths[" + std::to_string(index) + "]",
				s_Data.CascadeBlendWidths[index]);
			shader->BindTexture(
				"u_ShadowMaps[" + std::to_string(index) + "]",
				textureSlot + index,
				s_Data.Framebuffers[index]->GetDepthAttachmentRendererID());
		}
	}

	bool ShadowRenderer::IsEnabled()
	{
		return s_Data.Enabled;
	}

	void ShadowRenderer::SetCascadeDebugVisualization(bool enabled)
	{
		s_Data.CascadeDebugVisualization = enabled;
	}

	bool ShadowRenderer::IsCascadeDebugVisualizationEnabled()
	{
		return s_Data.CascadeDebugVisualization;
	}

	bool ShadowRenderer::ShouldCastShadow(MaterialAlphaMode alphaMode)
	{
		return alphaMode != MaterialAlphaMode::Blend;
	}

	ShadowRenderer::Statistics ShadowRenderer::GetStatistics()
	{
		return s_Data.Stats;
	}
}
