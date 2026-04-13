#include "glpch.h"
#include "Texture.h"

#include "Platform/OpenGL/OpenGLTexture2D.h"
#include "Glimmer/Renderer/Renderer.h"

namespace gl {

	std::shared_ptr<Texture2D> Texture2D::Create(const std::string& path)
	{
		// 以后这里可以根据 Renderer::GetAPI() 进行分支切换
		return CreateRef<OpenGLTexture2D>(path);
	}

}
