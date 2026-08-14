#pragma once

#include "Glimmer/Asset/MeshSource.h"

#include <filesystem>
#include <string>

namespace gl {

	struct ModelImportResult
	{
		MeshSource Source;
		std::string Error;

		explicit operator bool() const
		{
			return Error.empty() && Source.IsValid();
		}
	};

	class ModelImporter
	{
	public:
		static bool SupportsSource(const std::filesystem::path& path);
		static ModelImportResult Import(const std::filesystem::path& path);
	};

}
