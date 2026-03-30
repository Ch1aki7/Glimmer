#include "glpch.h"
#include "RenderCommand.h"
#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace gl {

	// 核心：在这里决定到底用哪个 API 实例
	RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI();

}