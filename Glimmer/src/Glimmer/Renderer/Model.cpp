#include "glpch.h"
#include "Model.h"
#include "Glimmer/Renderer/Renderer.h"
#include "tiny_obj_loader.h"
#include <unordered_map>

namespace gl {

	Model::Model(const std::string& path)
	{
		tinyobj::ObjReaderConfig reader_config;
		reader_config.mtl_search_path = "./assets/models"; // 材质搜索路径

		tinyobj::ObjReader reader;
		if (!reader.ParseFromFile(path, reader_config)) {
			if (!reader.Error().empty()) {
				GL_CORE_ERROR("TinyObjLoader Error: {0}", reader.Error());
			}
			return;
		}

		auto& attrib = reader.GetAttrib();
		auto& shapes = reader.GetShapes();

		// 遍历模型中的每个物体（Shape）
		for (size_t s = 0; s < shapes.size(); s++) {
			std::vector<Vertex> vertices;
			std::vector<uint32_t> indices;
			// 用于顶点去重，提升性能
			std::unordered_map<size_t, uint32_t> uniqueVertices{};

			for (const auto& index : shapes[s].mesh.indices) {
				Vertex vertex{};

				// 提取位置
				vertex.Position = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				// 提取法线
				if (index.normal_index >= 0) {
					vertex.Normal = {
						attrib.normals[3 * index.normal_index + 0],
						attrib.normals[3 * index.normal_index + 1],
						attrib.normals[3 * index.normal_index + 2]
					};
				}

				// 提取UV
				if (index.texcoord_index >= 0) {
					vertex.TexCoord = {
						attrib.texcoords[2 * index.texcoord_index + 0],
						attrib.texcoords[2 * index.texcoord_index + 1]
					};
				}
				// 简单的去重逻辑：如果这个顶点组合没出现过，就加入 vertices
				// 这里为了演示清晰使用线性填充，实际可用 Hash 优化
				indices.push_back((uint32_t)vertices.size());
				vertices.push_back(vertex);
			}

			m_Meshes.push_back(CreateRef<Mesh>(vertices, indices));
		}
		GL_CORE_INFO("Successfully loaded model: {0}", path);
	}

	void Model::Draw(const Ref<Shader>& shader, const glm::mat4& transform)
	{
		for (auto& mesh : m_Meshes)
		{
			// 利用你现有的 Renderer 系统提交绘制
			// 注意：这里暂时使用基础的 Submit，不走 2D 批处理
			shader->Bind();
			shader->UploadUniformMat4("u_ViewProjection", Renderer::GetViewProjection());
			shader->UploadUniformMat4("u_Transform", transform);
			mesh->Bind();
			RenderCommand::DrawIndexed(mesh->GetVertexArray(), mesh->GetIndexCount());
		}
	}

}
