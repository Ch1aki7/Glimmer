#pragma once
#include "Glimmer/Core/Core.h"
#include "Glimmer/Renderer/VertexArray.h"
#include "Glimmer/Renderer/Texture.h"

namespace gl {

	// 地形网格 —— 平面 Grid，高度在 VS 中采样 HeightMap 位移
	class TerrainMesh {
	public:
		TerrainMesh(uint32_t gridSize, float maxHeight);

		void Bind() const { m_VertexArray->Bind(); }
		uint32_t GetIndexCount() const { return m_IndexCount; }
		const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }

	private:
		Ref<VertexArray> m_VertexArray;
		uint32_t m_GridSize;
		uint32_t m_IndexCount;
	};

}
