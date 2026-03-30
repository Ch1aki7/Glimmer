#include "glpch.h"
#include "Buffer.h"
#include "Platform/OpenGL/OpenGLBuffer.h"

namespace gl {

    VertexBuffer* VertexBuffer::Create(float* vertices, uint32_t size) {
        // 未来可以在这里写 switch(Renderer::GetAPI()) 来切换平台
        return new OpenGLVertexBuffer(vertices, size);
    }

    IndexBuffer* IndexBuffer::Create(uint32_t* indices, uint32_t count) {
        return new OpenGLIndexBuffer(indices, count);
    }

}