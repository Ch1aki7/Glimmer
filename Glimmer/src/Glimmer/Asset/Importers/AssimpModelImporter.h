#pragma once

#include "ModelImporter.h"

namespace gl {

	class AssimpModelImporter
	{
	public:
		static ModelImportResult Import(const std::filesystem::path& path);
	};

}
