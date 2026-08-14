#pragma once

#include "ModelImporter.h"

namespace gl {

	class ObjModelImporter
	{
	public:
		static ModelImportResult Import(const std::filesystem::path& path);
	};

}
