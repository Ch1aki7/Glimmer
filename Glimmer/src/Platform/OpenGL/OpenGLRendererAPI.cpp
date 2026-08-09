#include "glpch.h"
#include "OpenGLRendererAPI.h"
#include <glad/glad.h>

namespace gl {
	void OpenGLRendererAPI::Init()
	{
		GL_PROFILE_FUNCTION();

		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
	}

	void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
	{
		glClearColor(color.r, color.g, color.b, color.a);
	}

	void OpenGLRendererAPI::Clear()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLRendererAPI::ClearDepth()
	{
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLRendererAPI::SetBlendEnabled(bool enabled)
	{
		if (enabled)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);
	}

	void OpenGLRendererAPI::SetBlendFunction(
		BlendFactor source, BlendFactor destination)
	{
		auto toOpenGL = [](BlendFactor factor) {
			switch (factor)
			{
			case BlendFactor::Zero: return GL_ZERO;
			case BlendFactor::One: return GL_ONE;
			case BlendFactor::SourceAlpha: return GL_SRC_ALPHA;
			case BlendFactor::OneMinusSourceAlpha: return GL_ONE_MINUS_SRC_ALPHA;
			default: return GL_ONE;
			}
		};
		glBlendFunc(toOpenGL(source), toOpenGL(destination));
	}

	void OpenGLRendererAPI::SetDepthWriteEnabled(bool enabled)
	{
		glDepthMask(enabled ? GL_TRUE : GL_FALSE);
	}

	void OpenGLRendererAPI::SetDepthFunction(DepthFunction function)
	{
		switch (function)
		{
		case DepthFunction::LessEqual:
			glDepthFunc(GL_LEQUAL);
			break;
		default:
			glDepthFunc(GL_LESS);
			break;
		}
	}

	void OpenGLRendererAPI::DrawIndexed(
		const Ref<VertexArray>& vertexArray, uint32_t indexCount)
	{
		vertexArray->Bind();
		uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
	}

	void OpenGLRendererAPI::DrawIndexedInstanced(
		const Ref<VertexArray>& vertexArray, uint32_t instanceCount, uint32_t indexCount)
	{
		vertexArray->Bind();
		uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
		glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr, instanceCount);
	}

}
