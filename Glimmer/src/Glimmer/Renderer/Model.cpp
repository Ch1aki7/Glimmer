#include "glpch.h"
#include "Model.h"
#include "Glimmer/Renderer/Renderer.h"
#include "tiny_obj_loader.h"
#include <unordered_map>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

// 1. 实现顶点去重需要的哈希结构
namespace std {
	template<> struct hash<gl::Vertex> {
		size_t operator()(gl::Vertex const& v) const {
			return ((hash<glm::vec3>()(v.Position) ^ (hash<glm::vec3>()(v.Normal) << 1)) >> 1) ^ (hash<glm::vec2>()(v.TexCoord) << 1);
		}
	};
}

namespace gl {

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

		// 核心重构：为了支持多材质，我们不能按 Shape 存，要按“材质ID”分网格
		// map 的 Key 是 material_id，Value 是该材质对应的顶点和索引数据
		struct MeshData {
			std::vector<Vertex> vertices;
			std::vector<uint32_t> indices;
			std::unordered_map<Vertex, uint32_t> uniqueVertices;
		};
		std::unordered_map<int, MeshData> materialToMeshData;
		for (const auto& shape : shapes) {
			size_t index_offset = 0;
			for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
				// 获取该面的材质 ID
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

					// 2. 真正的顶点去重
					if (meshData.uniqueVertices.count(vertex) == 0) {
						meshData.uniqueVertices[vertex] = (uint32_t)meshData.vertices.size();
						meshData.vertices.push_back(vertex);
					}
					meshData.indices.push_back(meshData.uniqueVertices[vertex]);
				}
				index_offset += 3;
			}
		}

		// 3. 将拆分好的材质数据转化为引擎的 Mesh 对象
		for (auto& [matID, data] : materialToMeshData) {
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

	void Model::Draw(const Ref<Shader>& shader, const glm::mat4& transform)
	{
		for (auto& mesh : m_Meshes)
		{
			shader->UploadUniformMat4("u_ViewProjection", Renderer::GetViewProjection());
			shader->UploadUniformMat4("u_Transform", transform);

			if (mesh->GetTexture()) {
				mesh->GetTexture()->Bind(0);
				shader->UploadUniformInt("u_Texture", 0);
			}
			else {
				// 如果没贴图，绑定引擎的白贴图防止变黑
				// Renderer2D::GetWhiteTexture()->Bind(0);
			}

			mesh->Bind();
			RenderCommand::DrawIndexed(mesh->GetVertexArray(), mesh->GetIndexCount());
		}
	}

}
