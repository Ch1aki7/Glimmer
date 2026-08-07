#include "glpch.h"
#include "OpenGLVertexArray.h"
#include <glad/glad.h>

namespace gl {

	static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
	{
		switch (type)
		{
		case ShaderDataType::Float:    return GL_FLOAT;
		case ShaderDataType::Float2:   return GL_FLOAT;
		case ShaderDataType::Float3:   return GL_FLOAT;
		case ShaderDataType::Float4:   return GL_FLOAT;
		case ShaderDataType::Mat3:     return GL_FLOAT;
		case ShaderDataType::Mat4:     return GL_FLOAT;
		case ShaderDataType::Int:      return GL_INT;
		case ShaderDataType::Int2:     return GL_INT;
		case ShaderDataType::Int3:     return GL_INT;
		case ShaderDataType::Int4:     return GL_INT;
		case ShaderDataType::Bool:     return GL_BOOL;
		}
		return 0;
	}

	static bool IsIntType(ShaderDataType type)
	{
		return type == ShaderDataType::Int
		    || type == ShaderDataType::Int2
		    || type == ShaderDataType::Int3
		    || type == ShaderDataType::Int4;
	}

	static bool IsMatrixType(ShaderDataType type)
	{
		return type == ShaderDataType::Mat3 || type == ShaderDataType::Mat4;
	}

	OpenGLVertexArray::OpenGLVertexArray()
	{
		GL_PROFILE_FUNCTION();
		glGenVertexArrays(1, &m_RendererID);
	}

	OpenGLVertexArray::~OpenGLVertexArray()
	{
		GL_PROFILE_FUNCTION();
		glDeleteVertexArrays(1, &m_RendererID);
	}

	void OpenGLVertexArray::Bind() const {
		GL_PROFILE_FUNCTION();
		glBindVertexArray(m_RendererID);
	}
	void OpenGLVertexArray::Unbind() const {
		GL_PROFILE_FUNCTION();
		glBindVertexArray(0);
	}

	void OpenGLVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer)
	{
		GL_PROFILE_FUNCTION();

		GL_CORE_ASSERT(vertexBuffer->GetLayout().GetElements().size(), "VertexBuffer has no layout!");

		glBindVertexArray(m_RendererID);
		vertexBuffer->Bind();

		const auto& layout = vertexBuffer->GetLayout();
		for (const auto& element : layout)
		{
			if (IsMatrixType(element.Type))
			{
				const uint32_t columnCount = element.GetComponentCount();
				for (uint32_t column = 0; column < columnCount; column++)
				{
					glEnableVertexAttribArray(m_VertexBufferIndex);
					glVertexAttribPointer(m_VertexBufferIndex, columnCount, GL_FLOAT,
						element.Normalized ? GL_TRUE : GL_FALSE, layout.GetStride(),
						(const void*)(element.Offset + sizeof(float) * columnCount * column));
					if (element.InputRate == BufferInputRate::PerInstance)
						glVertexAttribDivisor(m_VertexBufferIndex, 1);
					m_VertexBufferIndex++;
				}
				continue;
			}

			glEnableVertexAttribArray(m_VertexBufferIndex);
			if (IsIntType(element.Type))
				glVertexAttribIPointer(m_VertexBufferIndex,
					element.GetComponentCount(),
					ShaderDataTypeToOpenGLBaseType(element.Type),
					layout.GetStride(),
					(const void*)element.Offset);
			else
				glVertexAttribPointer(m_VertexBufferIndex,
					element.GetComponentCount(),
					ShaderDataTypeToOpenGLBaseType(element.Type),
					element.Normalized ? GL_TRUE : GL_FALSE,
					layout.GetStride(),
					(const void*)element.Offset);
			if (element.InputRate == BufferInputRate::PerInstance)
				glVertexAttribDivisor(m_VertexBufferIndex, 1);
			m_VertexBufferIndex++;
		}

		m_VertexBuffers.push_back(vertexBuffer);
	}

	void OpenGLVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
	{
		GL_PROFILE_FUNCTION();

		glBindVertexArray(m_RendererID);
		indexBuffer->Bind();

		m_IndexBuffer = indexBuffer;
	}

}
