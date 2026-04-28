#include "glpch.h"
#include "Mesh.h"
#include "Glimmer/Renderer/RenderCommand.h"

namespace gl {

	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
		: m_IndexCount((uint32_t)indices.size())
	{
		m_VertexArray = VertexArray::Create();

		// 创建顶点缓冲区 (VBO)
		auto vbo = VertexBuffer::Create((float*)vertices.data(), (uint32_t)(vertices.size() * sizeof(Vertex)));

		// 定义符合 Vertex 结构体的布局
		vbo->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float2, "a_TexCoord" }
			});
		m_VertexArray->AddVertexBuffer(vbo);

		// 创建索引缓冲区 (IBO)
		auto ibo = IndexBuffer::Create((uint32_t*)indices.data(), (uint32_t)indices.size());
		m_VertexArray->SetIndexBuffer(ibo);
	}

	void Mesh::Bind() const
	{
		m_VertexArray->Bind();
	}

}
