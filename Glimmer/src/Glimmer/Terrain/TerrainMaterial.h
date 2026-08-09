#pragma once

#include "Glimmer/Asset/Asset.h"
#include "Glimmer/Core/Core.h"

#include <array>
#include <filesystem>
#include <glm/glm.hpp>

namespace gl {

	enum class TerrainMaterialLayerType : uint8_t
	{
		Grass = 0,
		Soil,
		Rock,
		Snow,
		Count
	};

	const char* TerrainMaterialLayerTypeToString(TerrainMaterialLayerType type);

	struct TerrainMaterialLayer
	{
		glm::vec3 BaseColor{ 1.0f };
		AssetHandle AlbedoTexture{ 0 };
		AssetHandle NormalTexture{ 0 };
		AssetHandle AOTexture{ 0 };
		float Tiling = 0.12f;
		float Metallic = 0.0f;
		float Roughness = 0.8f;
		float NormalScale = 1.0f;
		float AOStrength = 1.0f;

		bool operator==(const TerrainMaterialLayer& other) const;
		bool operator!=(const TerrainMaterialLayer& other) const { return !(*this == other); }
	};

	struct TerrainMaterialProperties
	{
		std::array<TerrainMaterialLayer,
			static_cast<size_t>(TerrainMaterialLayerType::Count)> Layers;
		float TriplanarSharpness = 4.0f;
		float WeightContrast = 1.15f;
		float HeightInfluence = 0.65f;
		float SlopeInfluence = 1.0f;
		float CurvatureInfluence = 0.35f;
		float MoistureInfluence = 0.65f;

		bool operator==(const TerrainMaterialProperties& other) const;
		bool operator!=(const TerrainMaterialProperties& other) const { return !(*this == other); }
	};

	class TerrainMaterial
	{
	public:
		static Ref<TerrainMaterial> Create(const std::filesystem::path& path);
		static TerrainMaterialProperties CreateDefaultProperties();

		bool Reload();
		bool Save() const;

		const std::filesystem::path& GetPath() const { return m_Path; }
		TerrainMaterialProperties& GetProperties() { return m_Properties; }
		const TerrainMaterialProperties& GetProperties() const { return m_Properties; }
		void SetProperties(const TerrainMaterialProperties& properties);
		uint64_t GetVersion() const { return m_Version; }
		void MarkDirty() { ++m_Version; }

	private:
		explicit TerrainMaterial(std::filesystem::path path);

	private:
		std::filesystem::path m_Path;
		TerrainMaterialProperties m_Properties = CreateDefaultProperties();
		uint64_t m_Version = 0;
	};

}
