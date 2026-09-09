#pragma once

#include <filesystem>
#include <optional>
#include "Glimmer/Renderer/EditorCamera.h"

namespace gl {

	class EditorScenePreferences
	{
	public:
		static std::filesystem::path GetStoragePath();
		static std::optional<std::filesystem::path> LoadLastScene(
			const std::filesystem::path& projectRoot);
		static bool StoreLastScene(
			const std::filesystem::path& projectRoot,
			const std::optional<std::filesystem::path>& scenePath);
		static std::optional<EditorCameraState> LoadCameraState(
			const std::filesystem::path& projectRoot,
			const std::filesystem::path& scenePath);
		static bool StoreCameraState(
			const std::filesystem::path& projectRoot,
			const std::filesystem::path& scenePath,
			const EditorCameraState& state);
	};

}
