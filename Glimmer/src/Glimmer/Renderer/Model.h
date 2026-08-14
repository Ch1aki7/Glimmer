#pragma once
#include "Mesh.h"
#include "Glimmer/Renderer/Shader.h"
#include <filesystem>
#include <vector>

namespace gl {

	class Model {
	public:
		explicit Model(const std::filesystem::path& path);

		// 渲染模型的所有子网格
		const std::vector<Ref<Mesh>>& GetMeshes() const { return m_Meshes; }
		bool IsValid() const { return !m_Meshes.empty(); }

	private:
		std::vector<Ref<Mesh>> m_Meshes;
	};

}
