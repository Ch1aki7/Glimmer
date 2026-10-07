#include "glpch.h"
#include "GPUTimer.h"

#include "Glimmer/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLGPUTimer.h"

namespace gl {

	Ref<GPUTimer> GPUTimer::Create(bool timestampPairs)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			return nullptr;
		case RendererAPI::API::OpenGL:
			return CreateRef<OpenGLGPUTimer>(timestampPairs);
		case RendererAPI::API::Vulkan:
			return nullptr;
		}
		return nullptr;
	}

}
