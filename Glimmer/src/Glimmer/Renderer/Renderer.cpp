#include "glpch.h"
#include "Renderer.h"

namespace gl {

	void Renderer::BeginScene()
	{
		// 以后这里会接收摄像机，并计算 View-Projection 矩阵
	}

	void Renderer::EndScene()
	{
	}

	void Renderer::Submit(const std::shared_ptr<Shader>& shader, const std::shared_ptr<VertexArray>& vertexArray)
	{
		shader->Bind();
		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}

}