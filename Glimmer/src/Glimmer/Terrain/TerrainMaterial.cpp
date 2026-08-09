#include "glpch.h"
#include "TerrainMaterial.h"

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace gl {

	const char* TerrainMaterialLayerTypeToString(TerrainMaterialLayerType type)
	{
		switch (type)
		{
			case TerrainMaterialLayerType::Grass: return "Grass";
			case TerrainMaterialLayerType::Soil: return "Soil";
			case TerrainMaterialLayerType::Rock: return "Rock";
			case TerrainMaterialLayerType::Snow: return "Snow";
			default: return "Unknown";
		}
	}

	bool TerrainMaterialLayer::operator==(const TerrainMaterialLayer& other) const
	{
		return glm::all(glm::equal(BaseColor, other.BaseColor))
			&& AlbedoTexture == other.AlbedoTexture
			&& NormalTexture == other.NormalTexture
			&& AOTexture == other.AOTexture
			&& Tiling == other.Tiling
			&& Metallic == other.Metallic
			&& Roughness == other.Roughness
			&& NormalScale == other.NormalScale
			&& AOStrength == other.AOStrength;
	}

	bool TerrainMaterialProperties::operator==(const TerrainMaterialProperties& other) const
	{
		return Layers == other.Layers
			&& TriplanarSharpness == other.TriplanarSharpness
			&& WeightContrast == other.WeightContrast
			&& HeightInfluence == other.HeightInfluence
			&& SlopeInfluence == other.SlopeInfluence
			&& CurvatureInfluence == other.CurvatureInfluence
			&& MoistureInfluence == other.MoistureInfluence;
	}

	namespace {
		void EmitVec3(YAML::Emitter& output, const glm::vec3& value)
		{
			output << YAML::Flow << YAML::BeginSeq
				<< value.x << value.y << value.z << YAML::EndSeq;
		}

		void ReadVec3(const YAML::Node& node, glm::vec3& value)
		{
			if (node && node.IsSequence() && node.size() >= 3)
				value = { node[0].as<float>(), node[1].as<float>(), node[2].as<float>() };
		}

		void Clamp(TerrainMaterialProperties& properties)
		{
			properties.TriplanarSharpness = glm::clamp(properties.TriplanarSharpness, 1.0f, 16.0f);
			properties.WeightContrast = glm::clamp(properties.WeightContrast, 0.25f, 4.0f);
			properties.HeightInfluence = glm::clamp(properties.HeightInfluence, 0.0f, 2.0f);
			properties.SlopeInfluence = glm::clamp(properties.SlopeInfluence, 0.0f, 2.0f);
			properties.CurvatureInfluence = glm::clamp(properties.CurvatureInfluence, 0.0f, 2.0f);
			properties.MoistureInfluence = glm::clamp(properties.MoistureInfluence, 0.0f, 2.0f);
			for (auto& layer : properties.Layers)
			{
				layer.BaseColor = glm::max(layer.BaseColor, glm::vec3(0.0f));
				layer.Tiling = glm::clamp(layer.Tiling, 0.001f, 10.0f);
				layer.Metallic = glm::clamp(layer.Metallic, 0.0f, 1.0f);
				layer.Roughness = glm::clamp(layer.Roughness, 0.04f, 1.0f);
				layer.NormalScale = glm::clamp(layer.NormalScale, 0.0f, 2.0f);
				layer.AOStrength = glm::clamp(layer.AOStrength, 0.0f, 1.0f);
			}
		}

		bool ReplaceFileSafely(const std::filesystem::path& destination,
			const std::string& contents)
		{
			std::filesystem::path temporary = destination;
			temporary += ".tmp";
			std::filesystem::path backup = destination;
			backup += ".bak";
			std::error_code error;
			std::filesystem::remove(temporary, error);
			{
				std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
				if (!stream) return false;
				stream << contents;
				stream.flush();
				if (!stream.good()) return false;
			}
			const bool hadOriginal = std::filesystem::is_regular_file(destination, error);
			if (error) return false;
			if (hadOriginal)
			{
				std::filesystem::remove(backup, error);
				error.clear();
				std::filesystem::rename(destination, backup, error);
				if (error) return false;
			}
			std::filesystem::rename(temporary, destination, error);
			if (error)
			{
				if (hadOriginal)
				{
					std::error_code restoreError;
					std::filesystem::rename(backup, destination, restoreError);
				}
				return false;
			}
			if (hadOriginal)
				std::filesystem::remove(backup, error);
			return true;
		}
	}

	TerrainMaterial::TerrainMaterial(std::filesystem::path path)
		: m_Path(std::move(path)) {}

	TerrainMaterialProperties TerrainMaterial::CreateDefaultProperties()
	{
		TerrainMaterialProperties result;
		result.Layers[0].BaseColor = { 0.18f, 0.48f, 0.12f };
		result.Layers[0].Roughness = 0.88f;
		result.Layers[1].BaseColor = { 0.32f, 0.20f, 0.09f };
		result.Layers[1].Roughness = 0.92f;
		result.Layers[2].BaseColor = { 0.42f, 0.39f, 0.35f };
		result.Layers[2].Tiling = 0.08f;
		result.Layers[2].Roughness = 0.78f;
		result.Layers[3].BaseColor = { 0.92f, 0.94f, 0.98f };
		result.Layers[3].Tiling = 0.10f;
		result.Layers[3].Roughness = 0.55f;
		return result;
	}

	Ref<TerrainMaterial> TerrainMaterial::Create(const std::filesystem::path& path)
	{
		auto material = Ref<TerrainMaterial>(new TerrainMaterial(path));
		return material->Reload() ? material : nullptr;
	}

	void TerrainMaterial::SetProperties(const TerrainMaterialProperties& properties)
	{
		TerrainMaterialProperties value = properties;
		Clamp(value);
		if (m_Properties != value)
		{
			m_Properties = value;
			MarkDirty();
		}
	}

	bool TerrainMaterial::Reload()
	{
		try
		{
			const YAML::Node root = YAML::LoadFile(m_Path.string());
			const YAML::Node material = root["TerrainMaterial"];
			if (!material)
			{
				GL_CORE_ERROR("Terrain material has no TerrainMaterial root: {0}", m_Path.string());
				return false;
			}
			TerrainMaterialProperties properties = CreateDefaultProperties();
			if (material["TriplanarSharpness"]) properties.TriplanarSharpness = material["TriplanarSharpness"].as<float>();
			if (material["WeightContrast"]) properties.WeightContrast = material["WeightContrast"].as<float>();
			if (material["HeightInfluence"]) properties.HeightInfluence = material["HeightInfluence"].as<float>();
			if (material["SlopeInfluence"]) properties.SlopeInfluence = material["SlopeInfluence"].as<float>();
			if (material["CurvatureInfluence"]) properties.CurvatureInfluence = material["CurvatureInfluence"].as<float>();
			if (material["MoistureInfluence"]) properties.MoistureInfluence = material["MoistureInfluence"].as<float>();
			if (const auto layers = material["Layers"])
			{
				for (size_t index = 0; index < properties.Layers.size() && index < layers.size(); ++index)
				{
					const auto node = layers[index];
					auto& layer = properties.Layers[index];
					ReadVec3(node["BaseColor"], layer.BaseColor);
					if (node["AlbedoTexture"]) layer.AlbedoTexture = AssetHandle(node["AlbedoTexture"].as<uint64_t>());
					if (node["NormalTexture"]) layer.NormalTexture = AssetHandle(node["NormalTexture"].as<uint64_t>());
					if (node["AOTexture"]) layer.AOTexture = AssetHandle(node["AOTexture"].as<uint64_t>());
					if (node["Tiling"]) layer.Tiling = node["Tiling"].as<float>();
					if (node["Metallic"]) layer.Metallic = node["Metallic"].as<float>();
					if (node["Roughness"]) layer.Roughness = node["Roughness"].as<float>();
					if (node["NormalScale"]) layer.NormalScale = node["NormalScale"].as<float>();
					if (node["AOStrength"]) layer.AOStrength = node["AOStrength"].as<float>();
				}
			}
			Clamp(properties);
			if (m_Properties != properties || m_Version == 0)
			{
				m_Properties = properties;
				MarkDirty();
			}
			return true;
		}
		catch (const YAML::Exception& exception)
		{
			GL_CORE_ERROR("Terrain material parse error ({0}): {1}", m_Path.string(), exception.what());
			return false;
		}
	}

	bool TerrainMaterial::Save() const
	{
		YAML::Emitter output;
		output << YAML::BeginMap << YAML::Key << "TerrainMaterial" << YAML::Value << YAML::BeginMap;
		output << YAML::Key << "Version" << YAML::Value << 1;
		output << YAML::Key << "TriplanarSharpness" << YAML::Value << m_Properties.TriplanarSharpness;
		output << YAML::Key << "WeightContrast" << YAML::Value << m_Properties.WeightContrast;
		output << YAML::Key << "HeightInfluence" << YAML::Value << m_Properties.HeightInfluence;
		output << YAML::Key << "SlopeInfluence" << YAML::Value << m_Properties.SlopeInfluence;
		output << YAML::Key << "CurvatureInfluence" << YAML::Value << m_Properties.CurvatureInfluence;
		output << YAML::Key << "MoistureInfluence" << YAML::Value << m_Properties.MoistureInfluence;
		output << YAML::Key << "Layers" << YAML::Value << YAML::BeginSeq;
		for (size_t index = 0; index < m_Properties.Layers.size(); ++index)
		{
			const auto& layer = m_Properties.Layers[index];
			output << YAML::BeginMap;
			output << YAML::Key << "Name" << YAML::Value << TerrainMaterialLayerTypeToString(static_cast<TerrainMaterialLayerType>(index));
			output << YAML::Key << "BaseColor" << YAML::Value; EmitVec3(output, layer.BaseColor);
			output << YAML::Key << "AlbedoTexture" << YAML::Value << static_cast<uint64_t>(layer.AlbedoTexture);
			output << YAML::Key << "NormalTexture" << YAML::Value << static_cast<uint64_t>(layer.NormalTexture);
			output << YAML::Key << "AOTexture" << YAML::Value << static_cast<uint64_t>(layer.AOTexture);
			output << YAML::Key << "Tiling" << YAML::Value << layer.Tiling;
			output << YAML::Key << "Metallic" << YAML::Value << layer.Metallic;
			output << YAML::Key << "Roughness" << YAML::Value << layer.Roughness;
			output << YAML::Key << "NormalScale" << YAML::Value << layer.NormalScale;
			output << YAML::Key << "AOStrength" << YAML::Value << layer.AOStrength;
			output << YAML::EndMap;
		}
		output << YAML::EndSeq << YAML::EndMap << YAML::EndMap;
		if (!ReplaceFileSafely(m_Path, output.c_str()))
		{
			GL_CORE_ERROR("Could not write terrain material: {0}", m_Path.string());
			return false;
		}
		return true;
	}

}
