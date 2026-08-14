#pragma once
#include <glm/glm.hpp>
#include "Glimmer/Renderer/VertexArray.h"
#include "Glimmer/Renderer/Buffer.h"
#include "Glimmer/Renderer/Texture.h"
#include "Glimmer/Asset/MeshSource.h"

namespace gl {

	class Mesh {
	public:
		Mesh(const std::vector<MeshVertex>& vertices,
			const std::vector<uint32_t>& indices,
			Ref<Texture2D> texture);

		void Bind() const;
		uint32_t GetIndexCount() const { return m_IndexCount; }
		const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }
		const Ref<Texture2D>& GetTexture() const { return m_Texture; }
		bool HasBounds() const { return m_HasBounds; }
		const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
		const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }

	private:
		Ref<VertexArray> m_VertexArray;
		Ref<Texture2D> m_Texture;
		uint32_t m_IndexCount;
		glm::vec3 m_BoundsMin{ 0.0f };
		glm::vec3 m_BoundsMax{ 0.0f };
		bool m_HasBounds = false;
	};

}
