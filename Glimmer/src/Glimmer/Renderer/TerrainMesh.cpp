#include "glpch.h"
#include "TerrainMesh.h"
#include "Glimmer/Renderer/Buffer.h"

namespace gl {

	TerrainMesh::TerrainMesh(uint32_t gridSize, float maxHeight)
		: m_GridSize(gridSize)
	{
		// 顶点：每个格子一个顶点，存 (x, 0, z) + uv
		uint32_t vertCount = (gridSize + 1) * (gridSize + 1);
		float* vertices = new float[vertCount * 5]; // pos(3) + uv(2)

		float half = gridSize * 0.5f;
		for (uint32_t z = 0; z <= gridSize; z++)
		{
			for (uint32_t x = 0; x <= gridSize; x++)
			{
				uint32_t i = (z * (gridSize + 1) + x) * 5;
				vertices[i + 0] = (float)x - half;     // X
				vertices[i + 1] = 0.0f;                // Y (在 VS 中替换)
				vertices[i + 2] = (float)z - half;     // Z
				vertices[i + 3] = (float)x / gridSize; // U
				vertices[i + 4] = (float)z / gridSize; // V
			}
		}

		auto vbo = VertexBuffer::Create(vertices, vertCount * 5 * sizeof(float));
		vbo->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		});

		m_VertexArray = VertexArray::Create();
		m_VertexArray->AddVertexBuffer(vbo);

		// 索引：gridSize × gridSize 个 Quad = gridSize² × 6
		m_IndexCount = gridSize * gridSize * 6;
		uint32_t* indices = new uint32_t[m_IndexCount];

		uint32_t offset = 0;
		for (uint32_t z = 0; z < gridSize; z++)
		{
			for (uint32_t x = 0; x < gridSize; x++)
			{
				uint32_t tl = z * (gridSize + 1) + x;
				uint32_t tr = tl + 1;
				uint32_t bl = tl + (gridSize + 1);
				uint32_t br = bl + 1;

				indices[offset++] = tl; indices[offset++] = bl; indices[offset++] = tr;
				indices[offset++] = tr; indices[offset++] = bl; indices[offset++] = br;
			}
		}

		auto ibo = IndexBuffer::Create(indices, m_IndexCount);
		m_VertexArray->SetIndexBuffer(ibo);
		delete[] vertices;
		delete[] indices;
	}

}
