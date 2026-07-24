#include "glpch.h"
#include "Material.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace gl {

	namespace {

		void SerializeVec4(YAML::Emitter& output, const glm::vec4& value)
		{
			output << YAML::Flow << YAML::BeginSeq
				<< value.x << value.y << value.z << value.w
				<< YAML::EndSeq;
		}

		bool DeserializeVec4(const YAML::Node& node, glm::vec4& value)
		{
			if (!node || !node.IsSequence() || node.size() < 4)
				return false;

			value = {
				node[0].as<float>(),
				node[1].as<float>(),
				node[2].as<float>(),
				node[3].as<float>()
			};
			return true;
		}

	}

	Material::Material(std::filesystem::path path)
		: m_Path(std::move(path))
	{
	}

	Ref<Material> Material::Create(const std::filesystem::path& path)
	{
		auto material = Ref<Material>(new Material(path));
		return material->Reload() ? material : nullptr;
	}

	bool Material::Reload()
	{
		try
		{
			const YAML::Node root = YAML::LoadFile(m_Path.string());
			const YAML::Node material = root["Material"];
			if (!material)
			{
				GL_CORE_ERROR("Material file has no Material root: {0}", m_Path.string());
				return false;
			}

			MaterialProperties properties;
			AssetHandle shaderHandle{ 0 };

			if (material["Shader"])
				shaderHandle = AssetHandle(material["Shader"].as<uint64_t>());
			if (material["BaseColor"])
				DeserializeVec4(material["BaseColor"], properties.BaseColor);
			if (material["BaseColorTexture"])
				properties.BaseColorTexture =
					AssetHandle(material["BaseColorTexture"].as<uint64_t>());
			if (material["TilingFactor"])
				properties.TilingFactor = material["TilingFactor"].as<float>();
			if (material["Metallic"])
				properties.Metallic = material["Metallic"].as<float>();
			if (material["Roughness"])
				properties.Roughness = material["Roughness"].as<float>();

			properties.TilingFactor = glm::max(properties.TilingFactor, 0.01f);
			properties.Metallic = glm::clamp(properties.Metallic, 0.0f, 1.0f);
			properties.Roughness = glm::clamp(properties.Roughness, 0.04f, 1.0f);

			m_ShaderHandle = shaderHandle;
			m_Properties = properties;
			return true;
		}
		catch (const YAML::Exception& exception)
		{
			GL_CORE_ERROR("Material parse error ({0}): {1}", m_Path.string(), exception.what());
			return false;
		}
	}

	bool Material::Save() const
	{
		YAML::Emitter output;
		output << YAML::BeginMap;
		output << YAML::Key << "Material" << YAML::Value << YAML::BeginMap;
		output << YAML::Key << "Shader" << YAML::Value
			<< static_cast<uint64_t>(m_ShaderHandle);
		output << YAML::Key << "BaseColor" << YAML::Value;
		SerializeVec4(output, m_Properties.BaseColor);
		output << YAML::Key << "BaseColorTexture" << YAML::Value
			<< static_cast<uint64_t>(m_Properties.BaseColorTexture);
		output << YAML::Key << "TilingFactor" << YAML::Value << m_Properties.TilingFactor;
		output << YAML::Key << "Metallic" << YAML::Value << m_Properties.Metallic;
		output << YAML::Key << "Roughness" << YAML::Value << m_Properties.Roughness;
		output << YAML::EndMap;
		output << YAML::EndMap;

		std::ofstream stream(m_Path);
		if (!stream)
		{
			GL_CORE_ERROR("Could not write material: {0}", m_Path.string());
			return false;
		}

		stream << output.c_str();
		return true;
	}

}