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
		AssetHandle derivationShaderHandle)
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
		terrain.Specification.RenderShaderHandle = renderShaderHandle;
		terrain.Specification.GenerationShaderHandle = generationShaderHandle;
		terrain.Specification.ErosionShaderHandle = erosionShaderHandle;
		terrain.Specification.DerivationShaderHandle = derivationShaderHandle;
		terrain.Specification.TerrainMaterialHandle = AssetManager::ImportAsset(
			"assets/materials/DefaultTerrain.glterrainmat");
		return scene;
	}

}
