#include "glpch.h"
#include "ObjModelImporter.h"

#include "tiny_obj_loader.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <map>
#include <unordered_map>

namespace gl {

	namespace {

		struct MeshVertexHash
		{
			size_t operator()(const MeshVertex& vertex) const
			{
				return ((std::hash<glm::vec3>()(vertex.Position)
					^ (std::hash<glm::vec3>()(vertex.Normal) << 1)) >> 1)
					^ (std::hash<glm::vec3>()(vertex.Tangent) << 1)
					^ (std::hash<glm::vec2>()(vertex.TexCoord) << 1);
			}
		};

		struct MeshBuildData
		{
			std::vector<MeshVertex> Vertices;
			std::vector<uint32_t> Indices;
			std::unordered_map<MeshVertex, uint32_t, MeshVertexHash> UniqueVertices;
		};

		void ComputeTangents(
			std::vector<MeshVertex>& vertices,
			const std::vector<uint32_t>& indices)
		{
			if (indices.empty())
				return;

			std::vector<glm::vec3> tangentAccumulator(
				vertices.size(), glm::vec3(0.0f));
			for (size_t index = 0; index + 2 < indices.size(); index += 3)
			{
				MeshVertex& vertex0 = vertices[indices[index + 0]];
				MeshVertex& vertex1 = vertices[indices[index + 1]];
				MeshVertex& vertex2 = vertices[indices[index + 2]];

				const glm::vec3 edge1 = vertex1.Position - vertex0.Position;
				const glm::vec3 edge2 = vertex2.Position - vertex0.Position;
				const glm::vec2 deltaUV1 = vertex1.TexCoord - vertex0.TexCoord;
				const glm::vec2 deltaUV2 = vertex2.TexCoord - vertex0.TexCoord;
				const float determinant = deltaUV1.x * deltaUV2.y
					- deltaUV2.x * deltaUV1.y;
				if (glm::abs(determinant) <= 0.000001f)
					continue;

				const float inverseDeterminant = 1.0f / determinant;
				const glm::vec3 tangent = inverseDeterminant * (
					deltaUV2.y * edge1 - deltaUV1.y * edge2);
				for (size_t vertex = 0; vertex < 3; ++vertex)
					tangentAccumulator[indices[index + vertex]] += tangent;
			}

			for (size_t index = 0; index < vertices.size(); ++index)
			{
				const glm::vec3 normal =
					glm::dot(vertices[index].Normal, vertices[index].Normal) > 0.000001f
					? glm::normalize(vertices[index].Normal)
					: glm::vec3(0.0f, 1.0f, 0.0f);
				const glm::vec3 projected = tangentAccumulator[index]
					- normal * glm::dot(normal, tangentAccumulator[index]);
				if (glm::dot(projected, projected) > 0.000001f)
					vertices[index].Tangent = glm::normalize(projected);
				else
				{
					const glm::vec3 helper = glm::abs(normal.y) < 0.999f
						? glm::vec3(0.0f, 1.0f, 0.0f)
						: glm::vec3(1.0f, 0.0f, 0.0f);
					vertices[index].Tangent = glm::normalize(
						glm::cross(helper, normal));
				}
			}
		}

		MeshVertex ReadVertex(
			const tinyobj::attrib_t& attributes,
			const tinyobj::index_t& index)
		{
			MeshVertex vertex;
			if (index.vertex_index >= 0)
			{
				vertex.Position = {
					attributes.vertices[3 * index.vertex_index + 0],
					attributes.vertices[3 * index.vertex_index + 1],
					attributes.vertices[3 * index.vertex_index + 2]
				};
			}
			if (index.normal_index >= 0)
			{
				vertex.Normal = {
					attributes.normals[3 * index.normal_index + 0],
					attributes.normals[3 * index.normal_index + 1],
					attributes.normals[3 * index.normal_index + 2]
				};
			}
			if (index.texcoord_index >= 0)
			{
				vertex.TexCoord = {
					attributes.texcoords[2 * index.texcoord_index + 0],
					attributes.texcoords[2 * index.texcoord_index + 1]
				};
			}
			return vertex;
		}

	}

	ModelImportResult ObjModelImporter::Import(const std::filesystem::path& path)
	{
		ModelImportResult result;
		result.Source.SourcePath = path;

		tinyobj::ObjReaderConfig configuration;
		configuration.mtl_search_path = path.parent_path().string();
		configuration.triangulate = true;

		tinyobj::ObjReader reader;
		if (!reader.ParseFromFile(path.string(), configuration))
		{
			result.Error = reader.Error().empty()
				? "TinyObjLoader could not parse the model."
				: reader.Error();
			return result;
		}

		const auto& attributes = reader.GetAttrib();
		const auto& shapes = reader.GetShapes();
		const auto& materials = reader.GetMaterials();
		result.Source.Materials.reserve(materials.size());
		for (const tinyobj::material_t& material : materials)
		{
			MeshMaterialSource materialSource;
			materialSource.Name = material.name;
			if (!material.diffuse_texname.empty())
			{
				materialSource.BaseColorTexturePath =
					(path.parent_path() / material.diffuse_texname).lexically_normal();
			}
			result.Source.Materials.push_back(std::move(materialSource));
		}

		std::map<int, MeshBuildData> meshesByMaterial;
		for (const tinyobj::shape_t& shape : shapes)
		{
			size_t indexOffset = 0;
			for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face)
			{
				const size_t faceVertexCount = shape.mesh.num_face_vertices[face];
				if (faceVertexCount != 3)
				{
					indexOffset += faceVertexCount;
					continue;
				}

				const int materialIndex = shape.mesh.material_ids[face];
				MeshBuildData& meshData = meshesByMaterial[materialIndex];
				for (size_t vertexIndex = 0; vertexIndex < faceVertexCount; ++vertexIndex)
				{
					const MeshVertex vertex = ReadVertex(
						attributes, shape.mesh.indices[indexOffset + vertexIndex]);
					auto existing = meshData.UniqueVertices.find(vertex);
					if (existing == meshData.UniqueVertices.end())
					{
						const uint32_t newIndex =
							static_cast<uint32_t>(meshData.Vertices.size());
						meshData.Vertices.push_back(vertex);
						existing = meshData.UniqueVertices.emplace(vertex, newIndex).first;
					}
					meshData.Indices.push_back(existing->second);
				}
				indexOffset += faceVertexCount;
			}
		}

		result.Source.Submeshes.reserve(meshesByMaterial.size());
		for (auto& [materialIndex, data] : meshesByMaterial)
		{
			if (data.Vertices.empty() || data.Indices.empty())
				continue;
			ComputeTangents(data.Vertices, data.Indices);

			SubmeshSource submesh;
			submesh.Name = materialIndex >= 0
				&& static_cast<size_t>(materialIndex) < materials.size()
				? materials[materialIndex].name
				: "Default";
			submesh.MaterialIndex = materialIndex >= 0
				? static_cast<uint32_t>(materialIndex)
				: InvalidMaterialIndex;
			submesh.Vertices = std::move(data.Vertices);
			submesh.Indices = std::move(data.Indices);
			result.Source.Submeshes.push_back(std::move(submesh));
		}

		if (!result.Source.IsValid())
			result.Error = "OBJ model contains no valid triangle submeshes.";
		return result;
	}

}
