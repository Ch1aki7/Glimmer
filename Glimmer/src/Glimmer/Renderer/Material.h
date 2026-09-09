#pragma once

#include "Glimmer/Asset/Asset.h"
#include "Glimmer/Core/Core.h"
#include "Glimmer/Renderer/RendererAPI.h"

#include <glm/glm.hpp>
#include <filesystem>
#include <array>
#include <map>
#include <string>
#include <vector>

namespace gl {

	enum class MaterialAlphaMode : uint8_t
	{
		Opaque = 0,
		Mask,
		Blend
	};

	const char* MaterialAlphaModeToString(MaterialAlphaMode mode);
	MaterialAlphaMode MaterialAlphaModeFromString(const std::string& value);

	enum class MaterialPassQueue : uint8_t
	{
		Material = 0,
		Opaque
	};

	struct MaterialPass
	{
		std::string Name = "Forward";
		AssetHandle ShaderHandle{ 0 };
		int32_t Order = 100;
		CullMode Cull = CullMode::None;
		bool DepthWrite = true;
		MaterialPassQueue Queue = MaterialPassQueue::Material;
		std::map<std::string, float> FloatParameters;
		std::map<std::string, std::array<float, 4>> Float4Parameters;

		bool operator==(const MaterialPass& other) const
		{
			return Name == other.Name && ShaderHandle == other.ShaderHandle
				&& Order == other.Order && Cull == other.Cull
				&& DepthWrite == other.DepthWrite && Queue == other.Queue
				&& FloatParameters == other.FloatParameters
				&& Float4Parameters == other.Float4Parameters;
		}
	};

	struct MaterialProperties
	{
		glm::vec4 BaseColor{ 1.0f };
		AssetHandle BaseColorTexture{ 0 };
		AssetHandle NormalTexture{ 0 };
		AssetHandle AOTexture{ 0 };
		AssetHandle EmissiveTexture{ 0 };
		float TilingFactor = 1.0f;
		float Metallic = 0.0f;
		float Roughness = 0.5f;
		float NormalScale = 1.0f;
		float AOStrength = 1.0f;
		glm::vec3 EmissiveColor{ 1.0f };
		float EmissiveStrength = 0.0f;
		MaterialAlphaMode AlphaMode = MaterialAlphaMode::Opaque;
		float AlphaCutoff = 0.5f;

		bool operator==(const MaterialProperties& other) const
		{
			return glm::all(glm::equal(BaseColor, other.BaseColor))
				&& BaseColorTexture == other.BaseColorTexture
				&& NormalTexture == other.NormalTexture
				&& AOTexture == other.AOTexture
				&& EmissiveTexture == other.EmissiveTexture
				&& TilingFactor == other.TilingFactor
				&& Metallic == other.Metallic
				&& Roughness == other.Roughness
				&& NormalScale == other.NormalScale
				&& AOStrength == other.AOStrength
				&& glm::all(glm::equal(EmissiveColor, other.EmissiveColor))
				&& EmissiveStrength == other.EmissiveStrength
				&& AlphaMode == other.AlphaMode
				&& AlphaCutoff == other.AlphaCutoff;
		}
		bool operator!=(const MaterialProperties& other) const { return !(*this == other); }
	};

	struct MaterialState
	{
		AssetHandle ShaderHandle{ 0 };
		MaterialProperties Properties;
		std::vector<MaterialPass> Passes;

		bool operator==(const MaterialState& other) const
		{
			return ShaderHandle == other.ShaderHandle
				&& Properties == other.Properties && Passes == other.Passes;
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
		MaterialState GetState() const { return { m_ShaderHandle, m_Properties, m_Passes }; }
		void SetState(const MaterialState& state);
		uint64_t GetVersion() const { return m_Version; }
		void MarkDirty() { ++m_Version; }

		MaterialProperties& GetProperties() { return m_Properties; }
		const MaterialProperties& GetProperties() const { return m_Properties; }
		std::vector<MaterialPass>& GetPasses() { return m_Passes; }
		const std::vector<MaterialPass>& GetPasses() const { return m_Passes; }

	private:
		explicit Material(std::filesystem::path path);

	private:
		std::filesystem::path m_Path;
		AssetHandle m_ShaderHandle{ 0 };
		MaterialProperties m_Properties;
		std::vector<MaterialPass> m_Passes;
		uint64_t m_Version = 0;
	};

}
