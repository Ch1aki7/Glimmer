#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace gl {

	inline constexpr uint32_t InvalidMaterialIndex =
		std::numeric_limits<uint32_t>::max();

	struct MeshVertex
	{
		glm::vec3 Position{ 0.0f };
		glm::vec3 Normal{ 0.0f };
		glm::vec3 Tangent{ 0.0f };
		glm::vec2 TexCoord{ 0.0f };

		bool operator==(const MeshVertex& other) const
		{
			return Position == other.Position && Normal == other.Normal
				&& Tangent == other.Tangent && TexCoord == other.TexCoord;
		}
	};

	struct MeshMaterialSource
	{
		std::string Name;
		std::filesystem::path BaseColorTexturePath;
	};

	struct SubmeshSource
	{
		std::string Name;
		std::vector<MeshVertex> Vertices;
		std::vector<uint32_t> Indices;
		uint32_t MaterialIndex = InvalidMaterialIndex;

		bool IsValid() const
		{
			return !Vertices.empty() && !Indices.empty()
				&& Indices.size() % 3 == 0;
		}
	};

	struct MeshSource
	{
		std::filesystem::path SourcePath;
		std::vector<SubmeshSource> Submeshes;
		std::vector<MeshMaterialSource> Materials;

		bool IsValid() const
		{
			for (const SubmeshSource& submesh : Submeshes)
			{
				if (submesh.IsValid())
					return true;
			}
			return false;
		}
	};

}
