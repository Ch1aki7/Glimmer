#include "EditorScenePreferences.h"

#include "Glimmer/Core/Log.h"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <string>

namespace gl {
	namespace {
		constexpr const char* PreferencesHeader =
			"GLIMMER_EDITOR_SCENE_PREFERENCES 1";

		std::filesystem::path NormalizePath(const std::filesystem::path& path)
		{
			std::error_code error;
			auto normalized = std::filesystem::weakly_canonical(path, error);
			if (!error)
				return normalized;
			return std::filesystem::absolute(path, error).lexically_normal();
		}

		std::string ReadEnvironmentVariable(const char* name)
		{
#ifdef GL_PLATFORM_WINDOWS
			char* value = nullptr;
			size_t length = 0;
			if (_dupenv_s(&value, &length, name) != 0 || !value)
				return {};
			std::string result(value);
			std::free(value);
			return result;
#else
			const char* value = std::getenv(name);
			return value ? value : "";
#endif
		}
	}

	std::filesystem::path EditorScenePreferences::GetStoragePath()
	{
		if (const std::string overridePath = ReadEnvironmentVariable(
			"GLIMMER_EDITOR_PREFERENCES_PATH"); !overridePath.empty())
			return overridePath;

		std::string localData = ReadEnvironmentVariable("LOCALAPPDATA");
		if (localData.empty())
			localData = ReadEnvironmentVariable("XDG_CONFIG_HOME");
		if (!localData.empty())
			return std::filesystem::path(localData) / "Glimmer"
				/ "EditorScenePreferences.txt";

		return std::filesystem::current_path()
			/ ".glimmer-editor-scene-preferences.txt";
	}

	std::optional<std::filesystem::path>
	EditorScenePreferences::LoadLastScene(
		const std::filesystem::path& projectRoot)
	{
		std::ifstream stream(GetStoragePath(), std::ios::binary);
		if (!stream)
			return std::nullopt;

		std::string header;
		std::string storedProject;
		std::string storedScene;
		std::getline(stream, header);
		stream >> std::quoted(storedProject) >> std::quoted(storedScene);
		if (!stream || header != PreferencesHeader)
		{
			GL_CORE_WARN("Ignoring invalid editor scene preferences.");
			return std::nullopt;
		}

		if (NormalizePath(storedProject) != NormalizePath(projectRoot)
			|| storedScene.empty())
			return std::nullopt;

		return NormalizePath(storedScene);
	}

	bool EditorScenePreferences::StoreLastScene(
		const std::filesystem::path& projectRoot,
		const std::optional<std::filesystem::path>& scenePath)
	{
		const std::filesystem::path storagePath = GetStoragePath();
		std::error_code error;
		if (const auto parent = storagePath.parent_path(); !parent.empty())
		{
			std::filesystem::create_directories(parent, error);
			if (error)
			{
				GL_CORE_WARN("Could not create editor preferences directory: {0}",
					error.message());
				return false;
			}
		}

		std::ofstream stream(storagePath,
			std::ios::binary | std::ios::trunc);
		if (!stream)
			return false;

		stream << PreferencesHeader << '\n'
			<< std::quoted(NormalizePath(projectRoot).generic_string()) << '\n'
			<< std::quoted(scenePath
				? NormalizePath(*scenePath).generic_string() : std::string{}) << '\n';
		stream.flush();
		return stream.good();
	}

}
