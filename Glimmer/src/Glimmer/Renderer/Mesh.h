#pragma once
#include <glm/glm.hpp>
#include "Glimmer/Renderer/VertexArray.h"
#include "Glimmer/Renderer/Buffer.h"
#include "Glimmer/Renderer/Texture.h"
#include "Glimmer/Asset/MeshSource.h"

namespace gl {

	struct MeshMaterialTextures
	{
		Ref<Texture2D> BaseColor;
		Ref<Texture2D> Normal;
		Ref<Texture2D> Metallic;
		Ref<Texture2D> Roughness;
		Ref<Texture2D> AmbientOcclusion;
		Ref<Texture2D> Emissive;
	};

	class Mesh {
	public:
		Mesh(const std::vector<MeshVertex>& vertices,
			const std::vector<uint32_t>& indices,
			MeshMaterialTextures materialTextures = {});

		void Bind() const;
		uint32_t GetIndexCount() const { return m_IndexCount; }
		const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }
		const Ref<Texture2D>& GetTexture() const { return m_MaterialTextures.BaseColor; }
		const MeshMaterialTextures& GetMaterialTextures() const
		{
			return m_MaterialTextures;
		}
		bool HasBounds() const { return m_HasBounds; }
		const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
		const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }

	private:
		Ref<VertexArray> m_VertexArray;
		MeshMaterialTextures m_MaterialTextures;
		uint32_t m_IndexCount;
		glm::vec3 m_BoundsMin{ 0.0f };
		glm::vec3 m_BoundsMax{ 0.0f };
		bool m_HasBounds = false;
	};

}
