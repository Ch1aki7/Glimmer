#pragma once

#include "Glimmer/Asset/Asset.h"
#include "Glimmer/Core/Core.h"

#include <glm/glm.hpp>
#include <filesystem>

namespace gl {

	struct MaterialProperties
	{
		glm::vec4 BaseColor{ 1.0f };
		AssetHandle BaseColorTexture{ 0 };
		float TilingFactor = 1.0f;
		float Metallic = 0.0f;
		float Roughness = 0.5f;
	};

	class Material
	{
	public:
		static Ref<Material> Create(const std::filesystem::path& path);

		bool Reload();
		bool Save() const;

		const std::filesystem::path& GetPath() const { return m_Path; }
		AssetHandle GetShaderHandle() const { return m_ShaderHandle; }
		void SetShaderHandle(AssetHandle handle) { m_ShaderHandle = handle; }

		MaterialProperties& GetProperties() { return m_Properties; }
		const MaterialProperties& GetProperties() const { return m_Properties; }

	private:
		explicit Material(std::filesystem::path path);

	private:
		std::filesystem::path m_Path;
		AssetHandle m_ShaderHandle{ 0 };
		MaterialProperties m_Properties;
	};

}