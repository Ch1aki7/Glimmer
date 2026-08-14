#include "glpch.h"
#include "Mesh.h"
#include "Glimmer/Renderer/RenderCommand.h"

namespace gl {

	Mesh::Mesh(const std::vector<MeshVertex>& vertices,
		const std::vector<uint32_t>& indices,
		Ref<Texture2D> texture)
		: m_Texture(std::move(texture)), m_IndexCount((uint32_t)indices.size())
	{
		if (!vertices.empty())
		{
			m_BoundsMin = vertices.front().Position;
			m_BoundsMax = vertices.front().Position;
			for (const MeshVertex& vertex : vertices)
			{
				m_BoundsMin = glm::min(m_BoundsMin, vertex.Position);
				m_BoundsMax = glm::max(m_BoundsMax, vertex.Position);
			}
			m_HasBounds = true;
		}

		m_VertexArray = VertexArray::Create();

		auto vbo = VertexBuffer::Create((float*)vertices.data(),
			(uint32_t)(vertices.size() * sizeof(MeshVertex)));

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
