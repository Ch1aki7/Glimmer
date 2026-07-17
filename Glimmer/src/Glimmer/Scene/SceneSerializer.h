#pragma once
#include "Scene.h"

namespace gl {

	// 场景序列化器 —— 负责 Scene ↔ YAML 文件双向转换
	class SceneSerializer {
	public:
		SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene) {}

		void Serialize(const std::string& filepath);
		bool Deserialize(const std::string& filepath);

	private:
		Ref<Scene> m_Scene;
	};

}
