#include "glpch.h"
#include "UniformBuffer.h"

#include "Glimmer/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLUniformBuffer.h"

namespace gl {

	Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t binding)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:   GL_CORE_ASSERT(false, "RendererAPI::None not supported!"); return nullptr;
		case RendererAPI::API::OpenGL: return CreateRef<OpenGLUniformBuffer>(size, binding);
		case RendererAPI::API::Vulkan: GL_CORE_ASSERT(false, "Vulkan backend not yet implemented!"); return nullptr;
		}

		GL_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
