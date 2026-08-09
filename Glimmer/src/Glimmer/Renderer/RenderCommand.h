#pragma once
#include "RendererAPI.h"

namespace gl {

	class RenderCommand
	{
	public:
		inline static void Init()
		{
			s_RendererAPI->Init();
		}

		inline static void SetClearColor(const glm::vec4& color)
		{
			s_RendererAPI->SetClearColor(color);
		}

		inline static void Clear()
		{
			s_RendererAPI->Clear();
		}

		inline static void ClearDepth()
		{
			s_RendererAPI->ClearDepth();
		}

		inline static void SetDepthFunction(DepthFunction function)
		{
			s_RendererAPI->SetDepthFunction(function);
		}

		inline static void SetBlendEnabled(bool enabled)
		{
			s_RendererAPI->SetBlendEnabled(enabled);
		}

		inline static void SetBlendFunction(BlendFactor source, BlendFactor destination)
		{
			s_RendererAPI->SetBlendFunction(source, destination);
		}

		inline static void SetDepthWriteEnabled(bool enabled)
		{
			s_RendererAPI->SetDepthWriteEnabled(enabled);
		}

		inline static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t count = 0)
		{
			s_RendererAPI->DrawIndexed(vertexArray, count);
		}

		inline static void DrawIndexedInstanced(const Ref<VertexArray>& vertexArray,
			uint32_t instanceCount, uint32_t count = 0)
		{
			s_RendererAPI->DrawIndexedInstanced(vertexArray, instanceCount, count);
		}
	private:
		static RendererAPI* s_RendererAPI;
	};

}
