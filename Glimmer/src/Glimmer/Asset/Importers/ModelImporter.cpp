#include "glpch.h"
#include "ModelImporter.h"

#include "ObjModelImporter.h"

#include <algorithm>

namespace gl {

	namespace {

		std::string GetLowercaseExtension(const std::filesystem::path& path)
		{
			std::string extension = path.extension().string();
			std::transform(extension.begin(), extension.end(), extension.begin(),
				[](unsigned char character)
				{
					return static_cast<char>(std::tolower(character));
				});
			return extension;
		}

	}

	bool ModelImporter::SupportsSource(const std::filesystem::path& path)
	{
		return GetLowercaseExtension(path) == ".obj";
	}

	ModelImportResult ModelImporter::Import(const std::filesystem::path& path)
	{
		const std::string extension = GetLowercaseExtension(path);
		if (extension == ".obj")
			return ObjModelImporter::Import(path);

		ModelImportResult result;
		result.Error = "No model importer is registered for extension '"
			+ extension + "'.";
		return result;
	}

}
