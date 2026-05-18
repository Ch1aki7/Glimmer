#include "glpch.h"
#include "Mesh.h"
#include "Glimmer/Renderer/RenderCommand.h"

namespace gl {

	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, Ref<Texture2D> texture)
		: m_IndexCount((uint32_t)indices.size())
	{
		m_VertexArray = VertexArray::Create();

		auto vbo = VertexBuffer::Create((float*)vertices.data(), (uint32_t)(vertices.size() * sizeof(Vertex)));

		vbo->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float3, "a_Tangent" },
			{ ShaderDataType::Float2, "a_TexCoord" }
			});
		m_VertexArray->AddVertexBuffer(vbo);

		auto ibo = IndexBuffer::Create((uint32_t*)indices.data(), (uint32_t)indices.size());
		m_VertexArray->SetIndexBuffer(ibo);
	}

	void Mesh::Bind() const
	{
		m_VertexArray->Bind();
	}

}
