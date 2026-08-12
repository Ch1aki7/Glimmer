#include "glpch.h"
#include "TerrainMesh.h"
#include "Glimmer/Renderer/Buffer.h"

namespace gl {

	TerrainMesh::TerrainMesh(uint32_t gridSize)
		: m_GridSize(gridSize)
	{
		GL_CORE_ASSERT(gridSize > 0, "Terrain grid size must be greater than zero");
		// Surface vertices plus four duplicated boundary strips for skirts.
		const uint32_t surfaceVertexCount = (gridSize + 1) * (gridSize + 1);
		const uint32_t skirtVertexCount = 4 * (gridSize + 1);
		const uint32_t vertexCount = surfaceVertexCount + skirtVertexCount;
		std::vector<float> vertices(vertexCount * 6); // pos(3) + uv(2) + skirt(1)

		float half = gridSize * 0.5f;
		for (uint32_t z = 0; z <= gridSize; z++)
		{
			for (uint32_t x = 0; x <= gridSize; x++)
			{
				uint32_t i = (z * (gridSize + 1) + x) * 6;
				vertices[i + 0] = (float)x - half;     // X
				vertices[i + 1] = 0.0f;                // Y (在 VS 中替换)
				vertices[i + 2] = (float)z - half;     // Z
				vertices[i + 3] = (float)x / gridSize; // U
				vertices[i + 4] = (float)z / gridSize; // V
				vertices[i + 5] = 0.0f;
			}
		}
		uint32_t nextSkirtVertex = surfaceVertexCount;
		const auto addSkirtVertex = [&](uint32_t surfaceIndex) {
			const uint32_t source = surfaceIndex * 6;
			const uint32_t target = nextSkirtVertex * 6;
			for (uint32_t component = 0; component < 5; ++component)
				vertices[target + component] = vertices[source + component];
			vertices[target + 5] = 1.0f;
			return nextSkirtVertex++;
		};

		// 索引：gridSize × gridSize 个 Quad = gridSize² × 6
		std::vector<uint32_t> indices;
		indices.reserve(gridSize * gridSize * 6 + gridSize * 4 * 6);
		for (uint32_t z = 0; z < gridSize; z++)
		{
			for (uint32_t x = 0; x < gridSize; x++)
			{
				uint32_t tl = z * (gridSize + 1) + x;
				uint32_t tr = tl + 1;
				uint32_t bl = tl + (gridSize + 1);
				uint32_t br = bl + 1;

				indices.insert(indices.end(), { tl, bl, tr, tr, bl, br });
			}
		}
		const auto addEdge = [&](const std::vector<uint32_t>& surface,
			bool reverseWinding) {
			std::vector<uint32_t> skirt;
			skirt.reserve(surface.size());
			for (uint32_t vertex : surface)
				skirt.push_back(addSkirtVertex(vertex));
			for (uint32_t index = 0; index < gridSize; ++index)
			{
				const uint32_t a = surface[index];
				const uint32_t b = surface[index + 1];
				const uint32_t sa = skirt[index];
				const uint32_t sb = skirt[index + 1];
				if (reverseWinding)
					indices.insert(indices.end(), { a, sa, b, b, sa, sb });
				else
					indices.insert(indices.end(), { a, b, sa, b, sb, sa });
			}
		};
		std::vector<uint32_t> north, south, west, east;
		for (uint32_t index = 0; index <= gridSize; ++index)
		{
			north.push_back(index);
			south.push_back(gridSize * (gridSize + 1) + index);
			west.push_back(index * (gridSize + 1));
			east.push_back(index * (gridSize + 1) + gridSize);
		}
		addEdge(north, false);
		addEdge(south, true);
		addEdge(west, true);
		addEdge(east, false);

		auto vbo = VertexBuffer::Create(vertices.data(),
			static_cast<uint32_t>(vertices.size() * sizeof(float)));
		vbo->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" },
			{ ShaderDataType::Float, "a_Skirt" }
		});
		m_VertexArray = VertexArray::Create();
		m_VertexArray->AddVertexBuffer(vbo);
		m_IndexCount = static_cast<uint32_t>(indices.size());
		auto ibo = IndexBuffer::Create(indices.data(), m_IndexCount);
		m_VertexArray->SetIndexBuffer(ibo);
	}

}
