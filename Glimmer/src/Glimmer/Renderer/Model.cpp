#include "glpch.h"
#include "Model.h"
#include "Glimmer/Renderer/Renderer.h"
#include "tiny_obj_loader.h"
#include <unordered_map>

namespace gl {

	Model::Model(const std::string& path)
	{
		tinyobj::ObjReaderConfig reader_config;
		size_t lastSlash = path.find_last_of("/\\");
		std::string directory = (lastSlash == std::string::npos) ? "" : path.substr(0, lastSlash + 1);
		reader_config.mtl_search_path = directory;

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

		// 遍历模型中的每个物体（Shape）
		for (size_t s = 0; s < shapes.size(); s++) {
			std::vector<Vertex> vertices;
			std::vector<uint32_t> indices;

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
				indices.push_back((uint32_t)vertices.size());
				vertices.push_back(vertex);
			}
			Ref<Texture2D> meshTexture = nullptr;

			// 遍历该 shape 下的所有 material_id，找到第一个有效的
			int matID = -1;
			for (auto id : shapes[s].mesh.material_ids) {
				if (id >= 0) {
					matID = id;
					break;
				}
			}

			if (matID >= 0 && matID < (int)materials.size()) {
				std::string texName = materials[matID].diffuse_texname;
				if (!texName.empty()) {
					std::string fullTexPath = directory + texName;
					meshTexture = Texture2D::Create(fullTexPath);
					GL_CORE_INFO("  -> Success! Binding Texture: {0}", fullTexPath);
				}
			}

			if (!meshTexture) {
				GL_CORE_WARN("  -> Shape '{0}' has NO material IDs!", shapes[s].name);
			}

			m_Meshes.push_back(CreateRef<Mesh>(vertices, indices, meshTexture));
		}
		GL_CORE_INFO("Successfully loaded model: {0}", path);
	}

	void Model::Draw(const Ref<Shader>& shader, const glm::mat4& transform)
	{
		shader->Bind();
		for (auto& mesh : m_Meshes)
		{
			// 利用你现有的 Renderer 系统提交绘制
			// 注意：这里暂时使用基础的 Submit，不走 2D 批处理
			shader->UploadUniformMat4("u_ViewProjection", Renderer::GetViewProjection());
			shader->UploadUniformMat4("u_Transform", transform);
			if (mesh->GetTexture())
				mesh->GetTexture()->Bind(0);
			mesh->Bind();
			RenderCommand::DrawIndexed(mesh->GetVertexArray(), mesh->GetIndexCount());
		}
	}

}
