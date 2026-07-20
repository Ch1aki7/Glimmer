#include "glpch.h"
#include "Framebuffer.h"

#include "Glimmer/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLFramebuffer.h"

namespace gl {

	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:    GL_CORE_ASSERT(false, "RendererAPI::None not supported!"); return nullptr;
		case RendererAPI::API::OpenGL:  return CreateRef<OpenGLFramebuffer>(spec);
		case RendererAPI::API::Vulkan:  GL_CORE_ASSERT(false, "Vulkan backend not yet implemented!"); return nullptr;
		}

		GL_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
