#pragma once
#include <glm/glm.hpp>
#include "Glimmer/Renderer/VertexArray.h"
#include "Glimmer/Renderer/Buffer.h"
#include "Glimmer/Renderer/Texture.h"

namespace gl {

	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoord;
	};

	class Mesh {
	public:
		Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, Ref<Texture2D> texture);

		void Bind() const;
		uint32_t GetIndexCount() const { return m_IndexCount; }
		const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }
		const Ref<Texture2D>& GetTexture() const { return m_Texture; }

	private:
		Ref<VertexArray> m_VertexArray;
		Ref<Texture2D> m_Texture;
		uint32_t m_IndexCount;
	};

}
