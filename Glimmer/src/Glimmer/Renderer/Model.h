#pragma once
#include "Mesh.h"
#include "Glimmer/Renderer/Shader.h"
#include <vector>
#include <string>

namespace gl {

	class Model {
	public:
		Model(const std::string& path);

		// 渲染模型的所有子网格
		void Draw(const Ref<Shader>& shader, const glm::mat4& transform);

	private:
		std::vector<Ref<Mesh>> m_Meshes;
	};

}
