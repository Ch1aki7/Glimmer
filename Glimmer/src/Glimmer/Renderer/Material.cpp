#include "glpch.h"
#include "Material.h"

#include <yaml-cpp/yaml.h>
#include <algorithm>
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
		const char* CullModeToString(CullMode mode)
		{
			switch (mode)
			{
			case CullMode::Back: return "Back";
			case CullMode::Front: return "Front";
			default: return "None";
			}
		}

		CullMode CullModeFromString(const std::string& value)
		{
			if (value == "Back") return CullMode::Back;
			if (value == "Front") return CullMode::Front;
			return CullMode::None;
		}

		const char* PassQueueToString(MaterialPassQueue queue)
		{
			return queue == MaterialPassQueue::Opaque ? "Opaque" : "Material";
		}

		MaterialPassQueue PassQueueFromString(const std::string& value)
		{
			return value == "Opaque"
				? MaterialPassQueue::Opaque : MaterialPassQueue::Material;
		}
	}

	namespace {
		void SerializeVec3(YAML::Emitter& output, const glm::vec3& value)
		{
			output << YAML::Flow << YAML::BeginSeq
				<< value.x << value.y << value.z << YAML::EndSeq;
		}

		bool DeserializeVec3(const YAML::Node& node, glm::vec3& value)
		{
			if (!node || !node.IsSequence() || node.size() < 3)
				return false;
			value = { node[0].as<float>(), node[1].as<float>(), node[2].as<float>() };
			return true;
		}

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
		m_Passes = state.Passes;
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
			std::vector<MaterialPass> passes;

			if (material["Shader"])
				shaderHandle = AssetHandle(material["Shader"].as<uint64_t>());
			if (material["BaseColor"])
				DeserializeVec4(material["BaseColor"], properties.BaseColor);
			if (material["BaseColorTexture"])
				properties.BaseColorTexture =
					AssetHandle(material["BaseColorTexture"].as<uint64_t>());
			if (material["NormalTexture"])
				properties.NormalTexture = AssetHandle(material["NormalTexture"].as<uint64_t>());
			if (material["AOTexture"])
				properties.AOTexture = AssetHandle(material["AOTexture"].as<uint64_t>());
			if (material["EmissiveTexture"])
				properties.EmissiveTexture = AssetHandle(material["EmissiveTexture"].as<uint64_t>());
			if (material["TilingFactor"])
				properties.TilingFactor = material["TilingFactor"].as<float>();
			if (material["Metallic"])
				properties.Metallic = material["Metallic"].as<float>();
			if (material["Roughness"])
				properties.Roughness = material["Roughness"].as<float>();
			if (material["NormalScale"])
				properties.NormalScale = material["NormalScale"].as<float>();
			if (material["AOStrength"])
				properties.AOStrength = material["AOStrength"].as<float>();
			if (material["EmissiveColor"])
				DeserializeVec3(material["EmissiveColor"], properties.EmissiveColor);
			if (material["EmissiveStrength"])
				properties.EmissiveStrength = material["EmissiveStrength"].as<float>();
			if (material["AlphaMode"])
				properties.AlphaMode = MaterialAlphaModeFromString(
					material["AlphaMode"].as<std::string>());
			if (material["AlphaCutoff"])
				properties.AlphaCutoff = material["AlphaCutoff"].as<float>();
			if (const YAML::Node passNodes = material["Passes"];
				passNodes && passNodes.IsSequence())
			{
				for (const YAML::Node& node : passNodes)
				{
					MaterialPass pass;
					if (node["Name"]) pass.Name = node["Name"].as<std::string>();
					if (node["Shader"])
						pass.ShaderHandle = AssetHandle(node["Shader"].as<uint64_t>());
					if (node["Order"]) pass.Order = node["Order"].as<int32_t>();
					if (node["Cull"])
						pass.Cull = CullModeFromString(node["Cull"].as<std::string>());
					if (node["DepthWrite"])
						pass.DepthWrite = node["DepthWrite"].as<bool>();
					if (node["Queue"])
						pass.Queue = PassQueueFromString(node["Queue"].as<std::string>());
					if (const YAML::Node parameters = node["Parameters"];
						parameters && parameters.IsMap())
					{
						for (const auto& entry : parameters)
						{
							const std::string name = entry.first.as<std::string>();
							const YAML::Node value = entry.second;
							if (value.IsScalar())
								pass.FloatParameters[name] = value.as<float>();
							else if (value.IsSequence() && value.size() >= 4)
								pass.Float4Parameters[name] = {
									value[0].as<float>(), value[1].as<float>(),
									value[2].as<float>(), value[3].as<float>() };
						}
					}
					if (static_cast<uint64_t>(pass.ShaderHandle) != 0)
						passes.emplace_back(std::move(pass));
				}
				std::stable_sort(passes.begin(), passes.end(),
					[](const MaterialPass& left, const MaterialPass& right) {
						return left.Order < right.Order;
					});
			}

			properties.TilingFactor = glm::max(properties.TilingFactor, 0.01f);
			properties.Metallic = glm::clamp(properties.Metallic, 0.0f, 1.0f);
			properties.Roughness = glm::clamp(properties.Roughness, 0.04f, 1.0f);
			properties.NormalScale = glm::clamp(properties.NormalScale, 0.0f, 2.0f);
			properties.AOStrength = glm::clamp(properties.AOStrength, 0.0f, 1.0f);
			properties.EmissiveColor = glm::max(properties.EmissiveColor, glm::vec3(0.0f));
			properties.EmissiveStrength = glm::max(properties.EmissiveStrength, 0.0f);
			properties.AlphaCutoff = glm::clamp(properties.AlphaCutoff, 0.0f, 1.0f);

			const MaterialState loadedState{ shaderHandle, properties, passes };
			if (GetState() != loadedState || m_Version == 0)
			{
				m_ShaderHandle = shaderHandle;
				m_Properties = properties;
				m_Passes = passes;
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
		output << YAML::Key << "NormalTexture" << YAML::Value
			<< static_cast<uint64_t>(m_Properties.NormalTexture);
		output << YAML::Key << "AOTexture" << YAML::Value
			<< static_cast<uint64_t>(m_Properties.AOTexture);
		output << YAML::Key << "EmissiveTexture" << YAML::Value
			<< static_cast<uint64_t>(m_Properties.EmissiveTexture);
		output << YAML::Key << "TilingFactor" << YAML::Value << m_Properties.TilingFactor;
		output << YAML::Key << "Metallic" << YAML::Value << m_Properties.Metallic;
		output << YAML::Key << "Roughness" << YAML::Value << m_Properties.Roughness;
		output << YAML::Key << "NormalScale" << YAML::Value << m_Properties.NormalScale;
		output << YAML::Key << "AOStrength" << YAML::Value << m_Properties.AOStrength;
		output << YAML::Key << "EmissiveColor" << YAML::Value;
		SerializeVec3(output, m_Properties.EmissiveColor);
		output << YAML::Key << "EmissiveStrength" << YAML::Value
			<< m_Properties.EmissiveStrength;
		output << YAML::Key << "AlphaMode" << YAML::Value
			<< MaterialAlphaModeToString(m_Properties.AlphaMode);
		output << YAML::Key << "AlphaCutoff" << YAML::Value << m_Properties.AlphaCutoff;
		if (!m_Passes.empty())
		{
			output << YAML::Key << "Passes" << YAML::Value << YAML::BeginSeq;
			for (const MaterialPass& pass : m_Passes)
			{
				output << YAML::BeginMap;
				output << YAML::Key << "Name" << YAML::Value << pass.Name;
				output << YAML::Key << "Shader" << YAML::Value
					<< static_cast<uint64_t>(pass.ShaderHandle);
				output << YAML::Key << "Order" << YAML::Value << pass.Order;
				output << YAML::Key << "Cull" << YAML::Value
					<< CullModeToString(pass.Cull);
				output << YAML::Key << "DepthWrite" << YAML::Value << pass.DepthWrite;
				output << YAML::Key << "Queue" << YAML::Value
					<< PassQueueToString(pass.Queue);
				if (!pass.FloatParameters.empty() || !pass.Float4Parameters.empty())
				{
					output << YAML::Key << "Parameters" << YAML::Value << YAML::BeginMap;
					for (const auto& [name, value] : pass.FloatParameters)
						output << YAML::Key << name << YAML::Value << value;
					for (const auto& [name, value] : pass.Float4Parameters)
						output << YAML::Key << name << YAML::Value << YAML::Flow
							<< YAML::BeginSeq << value[0] << value[1]
							<< value[2] << value[3] << YAML::EndSeq;
					output << YAML::EndMap;
				}
				output << YAML::EndMap;
			}
			output << YAML::EndSeq;
		}
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
