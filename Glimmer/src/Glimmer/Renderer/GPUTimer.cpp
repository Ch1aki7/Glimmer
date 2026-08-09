#include "glpch.h"
#include "GPUTimer.h"

#include "Glimmer/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLGPUTimer.h"

namespace gl {

	Ref<GPUTimer> GPUTimer::Create()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			return nullptr;
		case RendererAPI::API::OpenGL:
			return CreateRef<OpenGLGPUTimer>();
		case RendererAPI::API::Vulkan:
			return nullptr;
		}
		return nullptr;
	}

}
