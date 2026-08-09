#pragma once

#include "Glimmer/Asset/Asset.h"
#include "Glimmer/Core/Core.h"

#include <glm/glm.hpp>
#include <filesystem>
#include <string>

namespace gl {

	enum class MaterialAlphaMode : uint8_t
	{
		Opaque = 0,
		Mask,
		Blend
	};

	const char* MaterialAlphaModeToString(MaterialAlphaMode mode);
	MaterialAlphaMode MaterialAlphaModeFromString(const std::string& value);

	struct MaterialProperties
	{
		glm::vec4 BaseColor{ 1.0f };
		AssetHandle BaseColorTexture{ 0 };
		float TilingFactor = 1.0f;
		float Metallic = 0.0f;
		float Roughness = 0.5f;
		MaterialAlphaMode AlphaMode = MaterialAlphaMode::Opaque;
		float AlphaCutoff = 0.5f;

		bool operator==(const MaterialProperties& other) const
		{
			return glm::all(glm::equal(BaseColor, other.BaseColor))
				&& BaseColorTexture == other.BaseColorTexture
				&& TilingFactor == other.TilingFactor
				&& Metallic == other.Metallic
				&& Roughness == other.Roughness
				&& AlphaMode == other.AlphaMode
				&& AlphaCutoff == other.AlphaCutoff;
		}
		bool operator!=(const MaterialProperties& other) const { return !(*this == other); }
	};

	struct MaterialState
	{
		AssetHandle ShaderHandle{ 0 };
		MaterialProperties Properties;

		bool operator==(const MaterialState& other) const
		{
			return ShaderHandle == other.ShaderHandle && Properties == other.Properties;
		}
		bool operator!=(const MaterialState& other) const { return !(*this == other); }
	};

	class Material
	{
	public:
		static Ref<Material> Create(const std::filesystem::path& path);

		bool Reload();
		bool Save() const;

		const std::filesystem::path& GetPath() const { return m_Path; }
		AssetHandle GetShaderHandle() const { return m_ShaderHandle; }
		void SetShaderHandle(AssetHandle handle);
		MaterialState GetState() const { return { m_ShaderHandle, m_Properties }; }
		void SetState(const MaterialState& state);
		uint64_t GetVersion() const { return m_Version; }
		void MarkDirty() { ++m_Version; }

		MaterialProperties& GetProperties() { return m_Properties; }
		const MaterialProperties& GetProperties() const { return m_Properties; }

	private:
		explicit Material(std::filesystem::path path);

	private:
		std::filesystem::path m_Path;
		AssetHandle m_ShaderHandle{ 0 };
		MaterialProperties m_Properties;
		uint64_t m_Version = 0;
	};

}
