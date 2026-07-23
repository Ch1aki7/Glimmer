#include "glpch.h"
#include "Texture.h"

#include "Glimmer/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLTexture2D.h"

namespace gl {

	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		return CreateRef<OpenGLTexture2D>(path);
	}

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height)
	{
		TextureSpecification specification;
		specification.Width = width;
		specification.Height = height;
		return Create(specification);
	}

	Ref<Texture2D> Texture2D::Create(const TextureSpecification& specification)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			GL_CORE_ASSERT(false, "RendererAPI::None does not support textures.");
			return nullptr;
		case RendererAPI::API::OpenGL:
			return CreateRef<OpenGLTexture2D>(specification);
		case RendererAPI::API::Vulkan:
			GL_CORE_ASSERT(false, "Vulkan textures are not implemented.");
			return nullptr;
		}
		return nullptr;
	}

}