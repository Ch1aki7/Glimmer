#include "TerrainValidationScene.h"

#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Core/Log.h"
#include "Glimmer/Scene/Components.h"
#include "Glimmer/Scene/Entity.h"
#include "Glimmer/Scene/Scene.h"
#include "Glimmer/Terrain/Terrain.h"

namespace gl {

	Ref<Scene> CreateTerrainValidationScene(
		AssetHandle skyboxHandle,
		AssetHandle renderShaderHandle,
		AssetHandle generationShaderHandle,
		AssetHandle erosionShaderHandle,
		AssetHandle derivationShaderHandle,
		uint32_t synthesisVersion, uint32_t dataVersion)
	{
		auto scene = CreateRef<Scene>();
		auto sunEntity = scene->CreateEntity("Sun");
		sunEntity.AddComponent<DirectionalLightComponent>();
		sunEntity.GetComponent<TransformComponent>().Rotation =
			{ -50.0f, 30.0f, 0.0f };

		auto pointLightEntity = scene->CreateEntity("Point Light");
		auto& pointLight = pointLightEntity.AddComponent<PointLightComponent>();
		pointLight.Intensity = 80.0f;
		pointLight.Range = 40.0f;
		pointLightEntity.GetComponent<TransformComponent>().Translation =
			{ 0.0f, 12.0f, 0.0f };

		scene->CreateEntity("Sky Light")
			.AddComponent<SkyLightComponent>(skyboxHandle);

		auto terrainEntity = scene->CreateEntity("Terrain");
		auto& terrain = terrainEntity.AddComponent<TerrainComponent>();
		ApplyTerrainPreset(terrain.Specification, TerrainPreset::Alpine);
		terrain.Specification.DataVersion = std::clamp(dataVersion, 1u, 2u);
		terrain.Specification.Noise.SynthesisVersion =
			std::clamp(synthesisVersion, 1u, 2u);
		terrain.Specification.RenderShaderHandle = renderShaderHandle;
		terrain.Specification.GenerationShaderHandle = generationShaderHandle;
		terrain.Specification.ErosionShaderHandle = erosionShaderHandle;
		terrain.Specification.DerivationShaderHandle = derivationShaderHandle;
		terrain.Specification.TerrainMaterialHandle = AssetManager::ImportAsset(
			"assets/materials/DefaultTerrain.glterrainmat");
		char* recipeFixture = nullptr;
		size_t recipeFixtureLength = 0;
		const bool enableRecipeFixture = _dupenv_s(&recipeFixture, &recipeFixtureLength,
			"GLIMMER_TERRAIN_RECIPE_FIXTURE") == 0 && recipeFixture && std::string(recipeFixture) == "1";
		std::free(recipeFixture);
		if (enableRecipeFixture)
		{
			terrain.Specification.DataVersion = 2;
			TerrainStamp platform;
			platform.ID = 1;
			platform.Shape = TerrainStampShape::Rectangle;
			platform.Size = { 256, 160 };
			platform.Height = terrain.Specification.HeightScale * 0.4f;
			platform.TransitionWidth = 64;
			platform.RotationDegrees = 23;
			TerrainStamp basin = platform;
			basin.ID = 2;
			basin.Shape = TerrainStampShape::Ellipse;
			basin.Operation = TerrainStampOperation::Add;
			basin.Center = { 180, -70 };
			basin.Size = { 200, 80 };
			basin.Height = -16;
			terrain.Specification.Recipe.Stamps = { platform, basin };
		}
		return scene;
	}

}
