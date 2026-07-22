#include "glpch.h"
#include "PixelBuffer.h"

#include "Glimmer/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLPixelBuffer.h"

namespace gl {

	Ref<PixelBuffer> PixelBuffer::Create(uint32_t width, uint32_t height, uint32_t channels)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:   GL_CORE_ASSERT(false, "RendererAPI::None not supported!"); return nullptr;
		case RendererAPI::API::OpenGL: return CreateRef<OpenGLPixelBuffer>(width, height, channels);
		case RendererAPI::API::Vulkan: GL_CORE_ASSERT(false, "Vulkan backend not yet implemented!"); return nullptr;
		}
		return nullptr;
	}

}
