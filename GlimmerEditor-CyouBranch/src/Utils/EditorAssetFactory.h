#pragma once

#include <filesystem>

namespace gl {

	enum class PrimitiveGeometry
	{
		Cube = 0,
		UVSphere,
		Plane
	};

	class EditorAssetFactory
	{
	public:
		static std::filesystem::path CreateFolder(
			const std::filesystem::path& directory);
		static std::filesystem::path CreateMaterial(
			const std::filesystem::path& directory);
		static std::filesystem::path CreateSkybox(
			const std::filesystem::path& directory);
		static std::filesystem::path CreateScene(
			const std::filesystem::path& directory);
		static std::filesystem::path CreateShader(
			const std::filesystem::path& directory);
		static std::filesystem::path CreateGeometry(
			const std::filesystem::path& directory,
			PrimitiveGeometry geometry);

	private:
		static std::filesystem::path GetUniquePath(
			const std::filesystem::path& directory,
			const std::string& baseName,
			const std::string& extension);
		static bool WriteTextFile(
			const std::filesystem::path& path,
			const std::string& contents);
	};

}
