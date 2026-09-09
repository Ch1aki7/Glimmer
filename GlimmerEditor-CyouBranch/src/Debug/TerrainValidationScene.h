#pragma once

#include "Glimmer/Asset/Asset.h"
#include "Glimmer/Core/Core.h"

namespace gl {

	class Scene;

	Ref<Scene> CreateTerrainValidationScene(
		AssetHandle skyboxHandle,
		AssetHandle renderShaderHandle,
		AssetHandle generationShaderHandle,
		AssetHandle erosionShaderHandle,
		AssetHandle derivationShaderHandle);

}
