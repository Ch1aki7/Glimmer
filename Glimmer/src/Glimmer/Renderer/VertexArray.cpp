#include "glpch.h"
#include "VertexArray.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace gl {

	VertexArray* VertexArray::Create()
	{
		// 暂时直接返回 OpenGL 版本
		return new OpenGLVertexArray();
	}

}