#include "glpch.h"
#include "Cubemap.h"

#include <yaml-cpp/yaml.h>

namespace gl {

	namespace {

		std::filesystem::path ReadFacePath(
			const YAML::Node& cubemap,
			const char* key,
			const std::filesystem::path& descriptorDirectory)
		{
			const YAML::Node value = cubemap[key];
			if (!value)
				return {};

			const std::string relativePath = value.as<std::string>();
			if (relativePath.empty())
				return {};

			return (descriptorDirectory / relativePath).lexically_normal();
		}

	}

	Cubemap::Cubemap(std::filesystem::path descriptorPath)
		: m_Path(std::move(descriptorPath))
	{
	}

	Ref<Cubemap> Cubemap::Create(const std::filesystem::path& descriptorPath)
	{
		Ref<Cubemap> cubemap(new Cubemap(descriptorPath));
		return cubemap->Reload() ? cubemap : nullptr;
	}

	bool Cubemap::Reload()
	{
		try
		{
			const YAML::Node root = YAML::LoadFile(m_Path.string());
			const YAML::Node cubemap = root["Cubemap"];
			if (!cubemap)
			{
				GL_CORE_ERROR(
					"Cubemap descriptor has no Cubemap root: {0}",
					m_Path.string());
				return false;
			}

			const std::filesystem::path directory = m_Path.parent_path();
			TextureCubeFileSpecification specification;
			specification.FacePaths = {
				ReadFacePath(cubemap, "Right", directory),
				ReadFacePath(cubemap, "Left", directory),
				ReadFacePath(cubemap, "Top", directory),
				ReadFacePath(cubemap, "Bottom", directory),
				ReadFacePath(cubemap, "Front", directory),
				ReadFacePath(cubemap, "Back", directory)
			};

			if (cubemap["ColorSpace"])
			{
				const std::string colorSpace =
					cubemap["ColorSpace"].as<std::string>();
				specification.ColorSpace = colorSpace == "Linear"
					? TextureColorSpace::Linear
					: TextureColorSpace::SRGB;
			}

			const YAML::Node fallback = cubemap["MissingFaceColor"];
			if (fallback && fallback.IsSequence() && fallback.size() >= 4)
			{
				for (size_t index = 0; index < 4; ++index)
					specification.MissingFaceColor[index] =
						static_cast<uint8_t>(glm::clamp(
							fallback[index].as<int>(), 0, 255));
			}

			Ref<TextureCube> texture = TextureCube::Create(specification);
			if (!texture)
				return false;

			m_Texture = std::move(texture);
			return true;
		}
		catch (const YAML::Exception& exception)
		{
			GL_CORE_ERROR(
				"Cubemap descriptor parse error ({0}): {1}",
				m_Path.string(),
				exception.what());
			return false;
		}
	}

}
