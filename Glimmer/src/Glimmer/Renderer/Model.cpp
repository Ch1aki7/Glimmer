#include "glpch.h"
#include "Model.h"
#include "tiny_obj_loader.h"
#include <unordered_map>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

// 顶点去重所需的哈希结构
namespace std {
	template<> struct hash<gl::Vertex> {
		size_t operator()(gl::Vertex const& v) const {
			return ((hash<glm::vec3>()(v.Position) ^ (hash<glm::vec3>()(v.Normal) << 1)) >> 1)
				^ (hash<glm::vec3>()(v.Tangent) << 1)
				^ (hash<glm::vec2>()(v.TexCoord) << 1);
		}
	};
}

namespace gl {

	// 按 UV 梯度生成切线；退化 UV 使用稳定正交基回退，避免 Normal Mapping 产生 NaN。
	static void ComputeTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	{
		if (indices.empty()) return;

		std::vector<glm::vec3> tanAccum(vertices.size(), glm::vec3(0.0f));

		for (size_t i = 0; i < indices.size(); i += 3)
		{
			Vertex& v0 = vertices[indices[i + 0]];
			Vertex& v1 = vertices[indices[i + 1]];
			Vertex& v2 = vertices[indices[i + 2]];

			glm::vec3 edge1 = v1.Position - v0.Position;
			glm::vec3 edge2 = v2.Position - v0.Position;
			glm::vec2 deltaUV1 = v1.TexCoord - v0.TexCoord;
			glm::vec2 deltaUV2 = v2.TexCoord - v0.TexCoord;

			const float determinant = deltaUV1.x * deltaUV2.y
				- deltaUV2.x * deltaUV1.y;
			if (glm::abs(determinant) <= 0.000001f)
				continue;
			const float f = 1.0f / determinant;

			glm::vec3 tangent;
			tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
			tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
			tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

			tanAccum[indices[i + 0]] += tangent;
			tanAccum[indices[i + 1]] += tangent;
			tanAccum[indices[i + 2]] += tangent;
		}

		// Gram-Schmidt 正交化：Tangent = normalize(T - N * dot(N, T))
		for (size_t i = 0; i < vertices.size(); i++)
		{
			const glm::vec3 normal = glm::dot(vertices[i].Normal, vertices[i].Normal) > 0.000001f
				? glm::normalize(vertices[i].Normal) : glm::vec3(0.0f, 1.0f, 0.0f);
			const glm::vec3 projected = tanAccum[i]
				- normal * glm::dot(normal, tanAccum[i]);
			if (glm::dot(projected, projected) > 0.000001f)
				vertices[i].Tangent = glm::normalize(projected);
			else
			{
				const glm::vec3 helper = glm::abs(normal.y) < 0.999f
					? glm::vec3(0.0f, 1.0f, 0.0f)
					: glm::vec3(1.0f, 0.0f, 0.0f);
				vertices[i].Tangent = glm::normalize(glm::cross(helper, normal));
			}
		}
	}

	Model::Model(const std::string& path)
	{
		tinyobj::ObjReaderConfig reader_config;
		size_t lastSlash = path.find_last_of("/\\");
		std::string directory = (lastSlash == std::string::npos) ? "" : path.substr(0, lastSlash + 1);
		reader_config.mtl_search_path = directory;
		reader_config.triangulate = true;

		tinyobj::ObjReader reader;
		if (!reader.ParseFromFile(path, reader_config)) {
			if (!reader.Error().empty()) {
				GL_CORE_ERROR("TinyObjLoader Error: {0}", reader.Error());
			}
			return;
		}

		auto& attrib = reader.GetAttrib();
		auto& shapes = reader.GetShapes();
		auto& materials = reader.GetMaterials();

		struct MeshData {
			std::vector<Vertex> vertices;
			std::vector<uint32_t> indices;
			std::unordered_map<Vertex, uint32_t> uniqueVertices;
		};
		std::unordered_map<int, MeshData> materialToMeshData;
		for (const auto& shape : shapes) {
			size_t index_offset = 0;
			for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
				int matID = shape.mesh.material_ids[f];
				auto& meshData = materialToMeshData[matID];

				for (size_t v = 0; v < 3; v++) {
					tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
					Vertex vertex{};

					vertex.Position = {
						attrib.vertices[3 * idx.vertex_index + 0],
						attrib.vertices[3 * idx.vertex_index + 1],
						attrib.vertices[3 * idx.vertex_index + 2]
					};
					if (idx.normal_index >= 0) {
						vertex.Normal = {
							attrib.normals[3 * idx.normal_index + 0],
							attrib.normals[3 * idx.normal_index + 1],
							attrib.normals[3 * idx.normal_index + 2]
						};
					}
					if (idx.texcoord_index >= 0) {
						vertex.TexCoord = {
							attrib.texcoords[2 * idx.texcoord_index + 0],
							attrib.texcoords[2 * idx.texcoord_index + 1]
						};
					}

					if (meshData.uniqueVertices.count(vertex) == 0) {
						meshData.uniqueVertices[vertex] = (uint32_t)meshData.vertices.size();
						meshData.vertices.push_back(vertex);
					}
					meshData.indices.push_back(meshData.uniqueVertices[vertex]);
				}
				index_offset += 3;
			}
		}

		for (auto& [matID, data] : materialToMeshData) {
			// 切向量在去重后计算，确保归一化正确
			ComputeTangents(data.vertices, data.indices);

			Ref<Texture2D> tex = nullptr;
			if (matID >= 0 && matID < materials.size()) {
				std::string texName = materials[matID].diffuse_texname;
				if (!texName.empty()) {
					tex = Texture2D::Create(directory + texName);
				}
			}
			m_Meshes.push_back(CreateRef<Mesh>(data.vertices, data.indices, tex));
		}
		GL_CORE_INFO("Model Loaded: {0}. Submeshes by material: {1}", path, m_Meshes.size());
	}
}
