#include "EditorScenePreferences.h"

#include "Glimmer/Core/Log.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <string>
#include <vector>

namespace gl {
	namespace {
		constexpr const char* PreferencesHeaderV1 =
			"GLIMMER_EDITOR_SCENE_PREFERENCES 1";
		constexpr const char* PreferencesHeaderV2 =
			"GLIMMER_EDITOR_SCENE_PREFERENCES 2";
		constexpr size_t MaximumCameraEntries = 64;

		struct CameraEntry
		{
			std::filesystem::path ScenePath;
			EditorCameraState State;
		};

		struct PreferencesDocument
		{
			std::optional<std::filesystem::path> LastScene;
			std::vector<CameraEntry> Cameras;
		};

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

		bool IsFinite(const EditorCameraState& state)
		{
			return std::isfinite(state.FocalPoint.x)
				&& std::isfinite(state.FocalPoint.y)
				&& std::isfinite(state.FocalPoint.z)
				&& std::isfinite(state.Distance)
				&& std::isfinite(state.Pitch)
				&& std::isfinite(state.Yaw);
		}

		std::optional<PreferencesDocument> LoadDocument(
			const std::filesystem::path& storagePath,
			const std::filesystem::path& projectRoot)
		{
			std::ifstream stream(storagePath, std::ios::binary);
			if (!stream)
				return PreferencesDocument{};

			std::string header;
			std::string storedProject;
			std::string storedScene;
			std::getline(stream, header);
			stream >> std::quoted(storedProject) >> std::quoted(storedScene);
			if (!stream || (header != PreferencesHeaderV1
				&& header != PreferencesHeaderV2))
			{
				GL_CORE_WARN("Ignoring invalid editor scene preferences.");
				return std::nullopt;
			}
			if (NormalizePath(storedProject) != NormalizePath(projectRoot))
				return std::nullopt;

			PreferencesDocument document;
			if (!storedScene.empty())
				document.LastScene = NormalizePath(storedScene);
			if (header == PreferencesHeaderV1)
				return document;

			size_t cameraCount = 0;
			stream >> cameraCount;
			if (!stream || cameraCount > MaximumCameraEntries)
				return std::nullopt;
			for (size_t index = 0; index < cameraCount; ++index)
			{
				std::string cameraScene;
				EditorCameraState state;
				stream >> std::quoted(cameraScene)
					>> state.FocalPoint.x >> state.FocalPoint.y
					>> state.FocalPoint.z >> state.Distance
					>> state.Pitch >> state.Yaw;
				if (!stream)
					return std::nullopt;
				if (!cameraScene.empty() && IsFinite(state))
					document.Cameras.push_back(
						{ NormalizePath(cameraScene), state });
			}
			return document;
		}

		bool SaveDocument(const std::filesystem::path& storagePath,
			const std::filesystem::path& projectRoot,
			const PreferencesDocument& document)
		{
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
			stream << PreferencesHeaderV2 << '\n'
				<< std::quoted(NormalizePath(projectRoot).generic_string()) << '\n'
				<< std::quoted(document.LastScene
					? NormalizePath(*document.LastScene).generic_string()
					: std::string{}) << '\n'
				<< document.Cameras.size() << '\n';
			stream << std::setprecision(9);
			for (const CameraEntry& entry : document.Cameras)
			{
				stream << std::quoted(
					NormalizePath(entry.ScenePath).generic_string()) << ' '
					<< entry.State.FocalPoint.x << ' '
					<< entry.State.FocalPoint.y << ' '
					<< entry.State.FocalPoint.z << ' '
					<< entry.State.Distance << ' '
					<< entry.State.Pitch << ' '
					<< entry.State.Yaw << '\n';
			}
			stream.flush();
			return stream.good();
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
		const auto document = LoadDocument(GetStoragePath(), projectRoot);
		return document ? document->LastScene : std::nullopt;
	}

	bool EditorScenePreferences::StoreLastScene(
		const std::filesystem::path& projectRoot,
		const std::optional<std::filesystem::path>& scenePath)
	{
		const std::filesystem::path storagePath = GetStoragePath();
		PreferencesDocument document =
			LoadDocument(storagePath, projectRoot).value_or(PreferencesDocument{});
		document.LastScene = scenePath;
		return SaveDocument(storagePath, projectRoot, document);
	}

	std::optional<EditorCameraState> EditorScenePreferences::LoadCameraState(
		const std::filesystem::path& projectRoot,
		const std::filesystem::path& scenePath)
	{
		const auto document = LoadDocument(GetStoragePath(), projectRoot);
		if (!document)
			return std::nullopt;
		const auto normalizedScene = NormalizePath(scenePath);
		for (const CameraEntry& entry : document->Cameras)
			if (entry.ScenePath == normalizedScene)
				return entry.State;
		return std::nullopt;
	}

	bool EditorScenePreferences::StoreCameraState(
		const std::filesystem::path& projectRoot,
		const std::filesystem::path& scenePath,
		const EditorCameraState& state)
	{
		if (scenePath.empty() || !IsFinite(state))
			return false;
		const std::filesystem::path storagePath = GetStoragePath();
		PreferencesDocument document =
			LoadDocument(storagePath, projectRoot).value_or(PreferencesDocument{});
		const auto normalizedScene = NormalizePath(scenePath);
		document.Cameras.erase(std::remove_if(
			document.Cameras.begin(), document.Cameras.end(),
			[&](const CameraEntry& entry) {
				return entry.ScenePath == normalizedScene;
			}), document.Cameras.end());
		document.Cameras.push_back({ normalizedScene, state });
		if (document.Cameras.size() > MaximumCameraEntries)
			document.Cameras.erase(document.Cameras.begin());
		return SaveDocument(storagePath, projectRoot, document);
	}

}
