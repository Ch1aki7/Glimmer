#pragma once
#include "Glimmer/Renderer/RendererAPI.h"

namespace gl {

	class OpenGLRendererAPI : public RendererAPI
	{
	public:
		virtual void Init() override;

		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void Clear() override;
		virtual void ClearDepth() override;

		virtual void SetBlendEnabled(bool enabled) override;
		virtual void SetBlendFunction(BlendFactor source, BlendFactor destination) override;
		virtual void SetDepthWriteEnabled(bool enabled) override;
		virtual void SetDepthFunction(DepthFunction function) override;
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) override;
		virtual void DrawIndexedInstanced(const Ref<VertexArray>& vertexArray, uint32_t instanceCount,
			uint32_t indexCount = 0) override;
	};

}
