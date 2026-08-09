#include "glpch.h"
#include "Material.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace gl {

	const char* MaterialAlphaModeToString(MaterialAlphaMode mode)
	{
		switch (mode)
		{
		case MaterialAlphaMode::Mask: return "Mask";
		case MaterialAlphaMode::Blend: return "Blend";
		default: return "Opaque";
		}
	}

	MaterialAlphaMode MaterialAlphaModeFromString(const std::string& value)
	{
		if (value == "Mask")
			return MaterialAlphaMode::Mask;
		if (value == "Blend")
			return MaterialAlphaMode::Blend;
		return MaterialAlphaMode::Opaque;
	}

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

		bool ReplaceFileSafely(
			const std::filesystem::path& destination,
			const std::string& contents)
		{
			std::filesystem::path temporary = destination;
			temporary += ".tmp";
			std::filesystem::path backup = destination;
			backup += ".bak";

			std::error_code error;
			std::filesystem::remove(temporary, error);
			error.clear();
			{
				std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
				if (!stream)
					return false;
				stream << contents;
				stream.flush();
				if (!stream.good())
				{
					stream.close();
					std::filesystem::remove(temporary, error);
					return false;
				}
			}

			const bool hadOriginal = std::filesystem::is_regular_file(destination, error);
			if (error)
			{
				std::filesystem::remove(temporary, error);
				return false;
			}

			if (hadOriginal)
			{
				std::filesystem::remove(backup, error);
				error.clear();
				std::filesystem::rename(destination, backup, error);
				if (error)
				{
					std::filesystem::remove(temporary, error);
					return false;
				}
			}

			std::filesystem::rename(temporary, destination, error);
			if (error)
			{
				const std::error_code replaceError = error;
				if (hadOriginal)
				{
					std::error_code restoreError;
					std::filesystem::rename(backup, destination, restoreError);
					if (restoreError)
						GL_CORE_ERROR(
							"Could not restore material backup {0}: {1}",
							backup.string(), restoreError.message());
				}
				std::filesystem::remove(temporary, error);
				GL_CORE_ERROR("Could not replace material file {0}: {1}",
					destination.string(), replaceError.message());
				return false;
			}

			if (hadOriginal)
			{
				std::filesystem::remove(backup, error);
				if (error)
					GL_CORE_WARN("Could not remove material backup {0}: {1}",
						backup.string(), error.message());
			}
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

	void Material::SetShaderHandle(AssetHandle handle)
	{
		if (m_ShaderHandle == handle)
			return;
		m_ShaderHandle = handle;
		MarkDirty();
	}

	void Material::SetState(const MaterialState& state)
	{
		if (GetState() == state)
			return;
		m_ShaderHandle = state.ShaderHandle;
		m_Properties = state.Properties;
		MarkDirty();
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
			if (material["AlphaMode"])
				properties.AlphaMode = MaterialAlphaModeFromString(
					material["AlphaMode"].as<std::string>());
			if (material["AlphaCutoff"])
				properties.AlphaCutoff = material["AlphaCutoff"].as<float>();

			properties.TilingFactor = glm::max(properties.TilingFactor, 0.01f);
			properties.Metallic = glm::clamp(properties.Metallic, 0.0f, 1.0f);
			properties.Roughness = glm::clamp(properties.Roughness, 0.04f, 1.0f);
			properties.AlphaCutoff = glm::clamp(properties.AlphaCutoff, 0.0f, 1.0f);

			const MaterialState loadedState{ shaderHandle, properties };
			if (GetState() != loadedState || m_Version == 0)
			{
				m_ShaderHandle = shaderHandle;
				m_Properties = properties;
				MarkDirty();
			}
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
		output << YAML::Key << "AlphaMode" << YAML::Value
			<< MaterialAlphaModeToString(m_Properties.AlphaMode);
		output << YAML::Key << "AlphaCutoff" << YAML::Value << m_Properties.AlphaCutoff;
		output << YAML::EndMap;
		output << YAML::EndMap;

		if (!ReplaceFileSafely(m_Path, output.c_str()))
		{
			GL_CORE_ERROR("Could not write material: {0}", m_Path.string());
			return false;
		}
		return true;
	}

}
