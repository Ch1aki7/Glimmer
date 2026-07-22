#include "glpch.h"
#include "ComputeShader.h"

#include "Glimmer/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLComputeShader.h"

namespace gl {

	Ref<ComputeShader> ComputeShader::Create(const std::string& filepath)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:   GL_CORE_ASSERT(false, "RendererAPI::None not supported!"); return nullptr;
		case RendererAPI::API::OpenGL: return CreateRef<OpenGLComputeShader>(filepath);
		case RendererAPI::API::Vulkan: GL_CORE_ASSERT(false, "Vulkan backend not yet implemented!"); return nullptr;
		}
		return nullptr;
	}

	void ComputeShader::Barrier()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::OpenGL:
			OpenGLComputeShader::Barrier();
			return;
		}
	}

}
