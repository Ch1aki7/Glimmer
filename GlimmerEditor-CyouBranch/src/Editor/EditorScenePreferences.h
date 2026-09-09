#pragma once

#include <filesystem>
#include <optional>

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
	};

}
