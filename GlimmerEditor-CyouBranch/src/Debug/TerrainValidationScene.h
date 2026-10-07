#pragma once

#include "Glimmer/Asset/Asset.h"
#include "Glimmer/Core/Core.h"

namespace gl {

	class Scene;
	bool ValidateTerrainRecipeEditorIntegration(const Ref<Scene>& scene);
	void SeedTerrainValidationWater(const Ref<Scene>& scene);
	void LogTerrainValidationResources(const Ref<Scene>& scene);

	Ref<Scene> CreateTerrainValidationScene(
		AssetHandle skyboxHandle,
		AssetHandle renderShaderHandle,
		AssetHandle generationShaderHandle,
		AssetHandle erosionShaderHandle,
		AssetHandle derivationShaderHandle,
		uint32_t synthesisVersion = 2, uint32_t dataVersion = 1);

}
