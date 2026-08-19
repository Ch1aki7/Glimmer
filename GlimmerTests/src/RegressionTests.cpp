#include "Glimmer/Core/Log.h"
#include "Glimmer/Asset/AssetManager.h"
#include "Glimmer/Asset/Importers/ModelImporter.h"
#include "Glimmer/Renderer/Material.h"
#include "Glimmer/Renderer/MaterialInstance.h"
#include "Glimmer/Renderer/EnvironmentMapLoader.h"
#include "Glimmer/Renderer/EnvironmentLighting.h"
#include "Glimmer/Renderer/ShadowRenderer.h"
#include "Glimmer/Renderer/TerrainRenderer.h"
#include "Glimmer/Scene/Components.h"
#include "Glimmer/Scene/Entity.h"
#include "Glimmer/Scene/Scene.h"
#include "Glimmer/Scene/SceneSerializer.h"
#include "Glimmer/Terrain/Terrain.h"
#include "Glimmer/Terrain/TerrainChunkLayout.h"
#include "Glimmer/Terrain/TerrainMaterial.h"
#include "Glimmer/Simulation/TerrainHydrologyRuntime.h"
#include "Glimmer/Simulation/TerrainClimateRuntime.h"
#include "Editor/EditorCommand.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <glm/gtc/matrix_transform.hpp>

namespace {

	class TestContext
	{
	public:
		void Check(bool condition, const std::string& message)
		{
			if (condition)
			{
				std::cout << "[PASS] " << message << '\n';
				return;
			}

			std::cerr << "[FAIL] " << message << '\n';
			++m_Failures;
		}

		int ExitCode() const { return m_Failures == 0 ? 0 : 1; }
		int FailureCount() const { return m_Failures; }

	private:
		int m_Failures = 0;
	};

	bool Near(float left, float right, float epsilon = 0.0001f)
	{
		return std::abs(left - right) <= epsilon;
	}

	bool Near(const glm::vec3& left, const glm::vec3& right)
	{
		return Near(left.x, right.x) && Near(left.y, right.y) && Near(left.z, right.z);
	}

	bool Near(const glm::vec2& left, const glm::vec2& right)
	{
		return Near(left.x, right.x) && Near(left.y, right.y);
	}

	bool Near(const glm::vec4& left, const glm::vec4& right)
	{
		return Near(left.x, right.x) && Near(left.y, right.y)
			&& Near(left.z, right.z) && Near(left.w, right.w);
	}

	std::filesystem::path FindRepositoryAsset(
		const std::filesystem::path& relativePath)
	{
		std::error_code error;
		std::filesystem::path directory =
			std::filesystem::absolute(std::filesystem::current_path(), error);
		while (!directory.empty())
		{
			const std::filesystem::path candidate = directory / relativePath;
			if (std::filesystem::is_regular_file(candidate, error))
				return candidate;

			const std::filesystem::path parent = directory.parent_path();
			if (parent == directory)
				break;
			directory = parent;
		}
		return {};
	}

	void TestModelImporterFoundation(
		TestContext& context,
		const std::filesystem::path& root)
	{
		const std::filesystem::path modelPath = root / "importer-triangle.obj";
		{
			std::ofstream stream(modelPath);
			stream << "v 0 0 0\n"
				<< "v 1 0 0\n"
				<< "v 0 1 0\n"
				<< "vt 0 0\n"
				<< "vt 1 0\n"
				<< "vt 0 1\n"
				<< "vn 0 0 1\n"
				<< "f 1/1/1 2/2/1 3/3/1\n";
		}

		const gl::ModelImportResult imported =
			gl::ModelImporter::Import(modelPath);
		context.Check(gl::ModelImporter::SupportsSource(modelPath)
			&& gl::ModelImporter::SupportsSource("static-model.fbx")
			&& !gl::ModelImporter::SupportsSource("future-model.gltf"),
			"model importer advertises OBJ and FBX only");
		context.Check(imported && imported.Source.Submeshes.size() == 1
			&& imported.Source.Submeshes.front().Vertices.size() == 3
			&& imported.Source.Submeshes.front().Indices.size() == 3,
			"OBJ importer produces a valid CPU MeshSource");
		const glm::vec3 tangent = imported
			? imported.Source.Submeshes.front().Vertices.front().Tangent
			: glm::vec3(0.0f);
		context.Check(std::isfinite(tangent.x) && std::isfinite(tangent.y)
			&& std::isfinite(tangent.z) && glm::length(tangent) > 0.9f,
			"OBJ MeshSource contains a finite normalized tangent");
	}

	void TestCerberusFBXImport(TestContext& context)
	{
		const std::filesystem::path modelPath =
			FindRepositoryAsset(
				"GlimmerEditor-CyouBranch/assets/models/Cerberus/Cerberus_LP.FBX");
		context.Check(!modelPath.empty(),
			"versioned Cerberus FBX regression asset is available");
		if (modelPath.empty())
			return;

		const gl::ModelImportResult imported =
			gl::ModelImporter::Import(modelPath);
		size_t vertexCount = 0;
		size_t indexCount = 0;
		bool finiteTangents = static_cast<bool>(imported);
		for (const gl::SubmeshSource& submesh : imported.Source.Submeshes)
		{
			vertexCount += submesh.Vertices.size();
			indexCount += submesh.Indices.size();
			for (const gl::MeshVertex& vertex : submesh.Vertices)
			{
				finiteTangents = finiteTangents
					&& std::isfinite(vertex.Tangent.x)
					&& std::isfinite(vertex.Tangent.y)
					&& std::isfinite(vertex.Tangent.z)
					&& glm::length(vertex.Tangent) > 0.9f;
			}
		}
		context.Check(imported && !imported.Source.Submeshes.empty()
			&& vertexCount > 0 && indexCount > 0 && indexCount % 3 == 0,
			"Cerberus FBX imports as valid static triangle submeshes");
		context.Check(finiteTangents,
			"Cerberus FBX vertices contain finite normalized tangents");

		bool hasCerberusPBRSet = false;
		for (const gl::MeshMaterialSource& material : imported.Source.Materials)
		{
			hasCerberusPBRSet = hasCerberusPBRSet
				|| (!material.BaseColorTexturePath.empty()
					&& !material.NormalTexturePath.empty()
					&& !material.MetallicTexturePath.empty()
					&& !material.RoughnessTexturePath.empty());
		}
		context.Check(hasCerberusPBRSet,
			"Cerberus sidecar Albedo Normal Metallic and Roughness are resolved");
	}

	bool SameTerrainSpecification(
		const gl::TerrainSpecification& left,
		const gl::TerrainSpecification& right)
	{
		const auto& leftNoise = left.Noise;
		const auto& rightNoise = right.Noise;
		return left.Procedural == right.Procedural
			&& left.Preset == right.Preset
			&& left.HeightMapResolution == right.HeightMapResolution
			&& left.MeshResolution == right.MeshResolution
			&& Near(left.HeightScale, right.HeightScale)
			&& left.HeightMapHandle == right.HeightMapHandle
			&& left.RenderShaderHandle == right.RenderShaderHandle
			&& left.GenerationShaderHandle == right.GenerationShaderHandle
			&& left.ErosionShaderHandle == right.ErosionShaderHandle
			&& left.DerivationShaderHandle == right.DerivationShaderHandle
			&& left.TerrainMaterialHandle == right.TerrainMaterialHandle
			&& leftNoise.Seed == rightNoise.Seed
			&& leftNoise.Octaves == rightNoise.Octaves
			&& Near(leftNoise.Frequency, rightNoise.Frequency)
			&& Near(leftNoise.Lacunarity, rightNoise.Lacunarity)
			&& Near(leftNoise.Persistence, rightNoise.Persistence)
			&& Near(leftNoise.DomainWarp, rightNoise.DomainWarp)
			&& Near(leftNoise.RidgeStrength, rightNoise.RidgeStrength)
			&& Near(leftNoise.ContinentScale, rightNoise.ContinentScale)
			&& Near(leftNoise.ErosionStrength, rightNoise.ErosionStrength)
			&& Near(leftNoise.DetailStrength, rightNoise.DetailStrength)
			&& Near(leftNoise.MountainDirection, rightNoise.MountainDirection)
			&& Near(leftNoise.MountainWidth, rightNoise.MountainWidth)
			&& Near(leftNoise.PlateauStrength, rightNoise.PlateauStrength)
			&& Near(leftNoise.GeologyBlend, rightNoise.GeologyBlend)
			&& Near(leftNoise.GeologyScale, rightNoise.GeologyScale)
			&& Near(leftNoise.RiftStrength, rightNoise.RiftStrength)
			&& Near(leftNoise.TrendStrength, rightNoise.TrendStrength)
			&& Near(leftNoise.Offset.x, rightNoise.Offset.x)
			&& Near(leftNoise.Offset.y, rightNoise.Offset.y)
			&& left.Authoring.EnableThermalErosion
				== right.Authoring.EnableThermalErosion
			&& left.Authoring.ThermalIterations
				== right.Authoring.ThermalIterations
			&& Near(left.Authoring.Talus, right.Authoring.Talus)
			&& Near(left.Authoring.ThermalStrength,
				right.Authoring.ThermalStrength);
	}

	class TemporaryDirectory
	{
	public:
		TemporaryDirectory()
		{
			m_Path = std::filesystem::temp_directory_path()
				/ ("GlimmerRegression-" + std::to_string(
					static_cast<uint64_t>(gl::UUID())));
			std::filesystem::create_directories(m_Path);
		}

		~TemporaryDirectory()
		{
			std::error_code error;
			std::filesystem::remove_all(m_Path, error);
		}

		const std::filesystem::path& Path() const { return m_Path; }

	private:
		std::filesystem::path m_Path;
	};

	void TestMaterialRoundTrip(TestContext& context, const std::filesystem::path& directory)
	{
		const std::filesystem::path path = directory / "MaterialRoundTrip.glmat";
		{
			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			output << "Material:\n  Shader: 11\n";
		}

		gl::Ref<gl::Material> material = gl::Material::Create(path);
		context.Check(static_cast<bool>(material), "legacy material loads without optional fields");
		if (!material)
			return;

		const gl::MaterialProperties defaults = material->GetProperties();
		context.Check(static_cast<uint64_t>(defaults.NormalTexture) == 0
			&& static_cast<uint64_t>(defaults.AOTexture) == 0
			&& static_cast<uint64_t>(defaults.EmissiveTexture) == 0,
			"legacy material restores empty extended texture handles");
		context.Check(Near(defaults.NormalScale, 1.0f)
			&& Near(defaults.AOStrength, 1.0f)
			&& Near(defaults.EmissiveStrength, 0.0f),
			"legacy material restores extended channel defaults");

		gl::MaterialState expected;
		expected.ShaderHandle = gl::AssetHandle(101);
		expected.Properties.BaseColor = { 0.12f, 0.34f, 0.56f, 0.78f };
		expected.Properties.BaseColorTexture = gl::AssetHandle(201);
		expected.Properties.NormalTexture = gl::AssetHandle(202);
		expected.Properties.AOTexture = gl::AssetHandle(203);
		expected.Properties.EmissiveTexture = gl::AssetHandle(204);
		expected.Properties.TilingFactor = 2.5f;
		expected.Properties.Metallic = 0.65f;
		expected.Properties.Roughness = 0.27f;
		expected.Properties.NormalScale = 1.4f;
		expected.Properties.AOStrength = 0.72f;
		expected.Properties.EmissiveColor = { 0.9f, 0.35f, 0.1f };
		expected.Properties.EmissiveStrength = 4.5f;
		expected.Properties.AlphaMode = gl::MaterialAlphaMode::Mask;
		expected.Properties.AlphaCutoff = 0.42f;

		material->SetState(expected);
		context.Check(material->Save(), "material saves atomically");
		gl::Ref<gl::Material> restored = gl::Material::Create(path);
		context.Check(static_cast<bool>(restored), "saved material reloads");
		if (!restored)
			return;

		const gl::MaterialState actual = restored->GetState();
		context.Check(static_cast<uint64_t>(actual.ShaderHandle) == 101,
			"material shader handle survives round trip");
		context.Check(Near(actual.Properties.BaseColor, expected.Properties.BaseColor)
			&& static_cast<uint64_t>(actual.Properties.BaseColorTexture) == 201
			&& static_cast<uint64_t>(actual.Properties.NormalTexture) == 202
			&& static_cast<uint64_t>(actual.Properties.AOTexture) == 203
			&& static_cast<uint64_t>(actual.Properties.EmissiveTexture) == 204,
			"material colors and texture handles survive round trip");
		context.Check(Near(actual.Properties.TilingFactor, 2.5f)
			&& Near(actual.Properties.Metallic, 0.65f)
			&& Near(actual.Properties.Roughness, 0.27f)
			&& Near(actual.Properties.NormalScale, 1.4f)
			&& Near(actual.Properties.AOStrength, 0.72f),
			"material scalar properties survive round trip");
		context.Check(Near(actual.Properties.EmissiveColor, expected.Properties.EmissiveColor)
			&& Near(actual.Properties.EmissiveStrength, 4.5f)
			&& actual.Properties.AlphaMode == gl::MaterialAlphaMode::Mask
			&& Near(actual.Properties.AlphaCutoff, 0.42f),
			"material emissive and alpha properties survive round trip");
	}

	void TestTerrainMaterialRoundTrip(TestContext& context,
		const std::filesystem::path& directory)
	{
		const std::filesystem::path path = directory / "TerrainRoundTrip.glterrainmat";
		{
			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			output << "TerrainMaterial:\n  Version: 1\n  Layers: []\n";
		}
		gl::Ref<gl::TerrainMaterial> material = gl::TerrainMaterial::Create(path);
		context.Check(static_cast<bool>(material),
			"terrain material loads from its distinct TerrainMaterial root");
		if (!material)
			return;
		auto properties = material->GetProperties();
		properties.TriplanarSharpness = 7.0f;
		properties.MoistureInfluence = 1.25f;
		properties.Layers[2].AlbedoTexture = gl::AssetHandle(7101);
		properties.Layers[2].NormalTexture = gl::AssetHandle(7102);
		properties.Layers[2].AOTexture = gl::AssetHandle(7103);
		properties.Layers[2].Tiling = 0.075f;
		properties.Layers[2].Roughness = 0.63f;
		material->SetProperties(properties);
		context.Check(material->Save(), "terrain material saves atomically");
		gl::Ref<gl::TerrainMaterial> restored = gl::TerrainMaterial::Create(path);
		context.Check(restored && restored->GetProperties() == material->GetProperties(),
			"all terrain layer textures and blend parameters survive round trip");
		std::ifstream input(path);
		const std::string contents((std::istreambuf_iterator<char>(input)), {});
		context.Check(contents.find("TerrainMaterial:") != std::string::npos
			&& contents.find("\nMaterial:") == std::string::npos,
			"terrain material serialization does not reuse or pollute .glmat layout");
	}

	void TestTerrainMaterialRegistry(TestContext& context,
		const std::filesystem::path& directory)
	{
		const auto regularPath = directory / "RegistryMaterial.glmat";
		const auto terrainPath = directory / "RegistryTerrain.glterrainmat";
		{
			std::ofstream regular(regularPath);
			regular << "Material:\n  Shader: 0\n";
			std::ofstream terrain(terrainPath);
			terrain << "TerrainMaterial:\n  Version: 1\n  Layers: []\n";
		}
		gl::AssetManager::Initialize(directory);
		const gl::AssetHandle regular = gl::AssetManager::ImportAsset(regularPath);
		const gl::AssetHandle terrain = gl::AssetManager::ImportAsset(terrainPath);
		context.Check(static_cast<uint64_t>(regular) != 0
			&& static_cast<uint64_t>(terrain) != 0 && regular != terrain,
			"material and terrain material import as distinct handles");
		context.Check(gl::AssetManager::GetMetadata(regular).Type == gl::AssetType::Material
			&& gl::AssetManager::GetMetadata(terrain).Type == gl::AssetType::TerrainMaterial,
			"asset registry preserves distinct material types");
		context.Check(gl::AssetManager::GetMaterial(terrain) == nullptr
			&& gl::AssetManager::GetTerrainMaterial(regular) == nullptr
			&& gl::AssetManager::GetTerrainMaterial(terrain) != nullptr,
			"typed caches reject cross-loading .glmat and .glterrainmat");
		gl::AssetManager::Shutdown();
	}

	void TestMaterialOverrideMerge(TestContext& context, const std::filesystem::path& directory)
	{
		const std::filesystem::path path = directory / "OverrideBase.glmat";
		{
			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			output << "Material:\n"
				<< "  Shader: 55\n"
				<< "  BaseColor: [0.2, 0.3, 0.4, 1.0]\n"
				<< "  TilingFactor: 3.0\n"
				<< "  Metallic: 0.25\n"
				<< "  Roughness: 0.8\n";
		}

		gl::Ref<gl::Material> material = gl::Material::Create(path);
		context.Check(static_cast<bool>(material), "override base material loads");
		if (!material)
			return;

		gl::MaterialOverrides overrides;
		overrides.Values.BaseColor = { 1.0f, 0.0f, 0.0f, 1.0f };
		overrides.Values.Roughness = -1.0f;
		overrides.Values.NormalScale = 3.0f;
		overrides.Values.AOStrength = 2.0f;
		overrides.Values.EmissiveColor = { -1.0f, 0.5f, 2.0f };
		overrides.Values.EmissiveStrength = -4.0f;
		overrides.Values.AlphaCutoff = 2.0f;
		overrides.Values.AOTexture = gl::AssetHandle(9001);
		overrides.SetEnabled(gl::MaterialOverride::Roughness, true);
		overrides.SetEnabled(gl::MaterialOverride::NormalScale, true);
		overrides.SetEnabled(gl::MaterialOverride::AOStrength, true);
		overrides.SetEnabled(gl::MaterialOverride::EmissiveColor, true);
		overrides.SetEnabled(gl::MaterialOverride::EmissiveStrength, true);
		overrides.SetEnabled(gl::MaterialOverride::AlphaCutoff, true);
		overrides.SetEnabled(gl::MaterialOverride::AOTexture, true);

		gl::MaterialInstance instance(material, overrides);
		const gl::MaterialProperties& properties = instance.GetProperties();
		context.Check(Near(properties.BaseColor, material->GetProperties().BaseColor),
			"disabled override leaves base value unchanged");
		context.Check(Near(properties.Roughness, 0.04f)
			&& Near(properties.NormalScale, 2.0f)
			&& Near(properties.AOStrength, 1.0f),
			"enabled scalar overrides use runtime clamps");
		context.Check(Near(properties.EmissiveColor, glm::vec3(0.0f, 0.5f, 2.0f))
			&& Near(properties.EmissiveStrength, 0.0f)
			&& Near(properties.AlphaCutoff, 1.0f),
			"emissive and alpha overrides use runtime clamps");
		context.Check(static_cast<uint64_t>(properties.AOTexture) == 9001,
			"enabled texture override replaces the base handle");
	}

	void TestSceneRoundTrip(TestContext& context, const std::filesystem::path& directory)
	{
		const gl::UUID entityUUID(0x123456789ABCDEF0ull);
		const std::filesystem::path path = directory / "MinimalScene.glimmer";
		gl::Ref<gl::Scene> source = gl::CreateRef<gl::Scene>();
		gl::Entity entity = source->CreateEntityWithUUID(entityUUID, "Regression Entity");
		auto& transform = entity.GetComponent<gl::TransformComponent>();
		transform.Translation = { 1.25f, -2.5f, 3.75f };
		transform.Rotation = { 10.0f, 20.0f, 30.0f };
		transform.Scale = { 2.0f, 3.0f, 4.0f };

		auto& model = entity.AddComponent<gl::ModelRendererComponent>();
		model.ModelHandle = gl::AssetHandle(5001);
		auto& material = entity.AddComponent<gl::MaterialComponent>();
		material.MaterialHandle = gl::AssetHandle(5002);
		material.Overrides.Values.NormalTexture = gl::AssetHandle(5003);
		material.Overrides.Values.AOTexture = gl::AssetHandle(5004);
		material.Overrides.Values.EmissiveTexture = gl::AssetHandle(5005);
		material.Overrides.Values.AOStrength = 0.6f;
		material.Overrides.Values.EmissiveColor = { 0.8f, 0.3f, 0.1f };
		material.Overrides.Values.EmissiveStrength = 3.0f;
		material.Overrides.SetEnabled(gl::MaterialOverride::NormalTexture, true);
		material.Overrides.SetEnabled(gl::MaterialOverride::AOTexture, true);
		material.Overrides.SetEnabled(gl::MaterialOverride::EmissiveTexture, true);
		material.Overrides.SetEnabled(gl::MaterialOverride::AOStrength, true);
		material.Overrides.SetEnabled(gl::MaterialOverride::EmissiveColor, true);
		material.Overrides.SetEnabled(gl::MaterialOverride::EmissiveStrength, true);

		auto& terrain = entity.AddComponent<gl::TerrainComponent>();
		terrain.Specification.Procedural = true;
		terrain.Specification.HeightMapResolution = 1024;
		terrain.Specification.MeshResolution = 192;
		terrain.Specification.HeightScale = 37.5f;
		terrain.Specification.HeightMapHandle = gl::AssetHandle(6001);
		terrain.Specification.RenderShaderHandle = gl::AssetHandle(6002);
		terrain.Specification.GenerationShaderHandle = gl::AssetHandle(6003);
		terrain.Specification.ErosionShaderHandle = gl::AssetHandle(6004);
		terrain.Specification.DerivationShaderHandle = gl::AssetHandle(6005);
		terrain.Specification.TerrainMaterialHandle = gl::AssetHandle(6006);
		terrain.Specification.Noise.Seed = 73;
		terrain.Specification.Noise.Octaves = 7;
		terrain.Specification.Noise.Frequency = 1.35f;
		terrain.Specification.Noise.Lacunarity = 2.4f;
		terrain.Specification.Noise.Persistence = 0.42f;
		terrain.Specification.Noise.DomainWarp = 0.8f;
		terrain.Specification.Noise.RidgeStrength = 0.65f;
		terrain.Specification.Noise.ContinentScale = 0.3f;
		terrain.Specification.Noise.ErosionStrength = 0.17f;
		terrain.Specification.Noise.DetailStrength = 0.09f;
		terrain.Specification.Noise.MountainDirection = -0.45f;
		terrain.Specification.Noise.MountainWidth = 0.21f;
		terrain.Specification.Noise.PlateauStrength = 0.31f;
		terrain.Specification.Noise.GeologyBlend = 0.74f;
		terrain.Specification.Noise.GeologyScale = 4.25f;
		terrain.Specification.Noise.RiftStrength = 0.19f;
		terrain.Specification.Noise.TrendStrength = 0.13f;
		terrain.Specification.Noise.Offset = { 4.0f, -2.0f };
		terrain.Specification.Authoring.EnableThermalErosion = true;
		terrain.Specification.Authoring.ThermalIterations = 31;
		terrain.Specification.Authoring.Talus = 0.014f;
		terrain.Specification.Authoring.ThermalStrength = 0.27f;
		terrain.Runtime = gl::CreateRef<gl::TerrainRuntime>();
		terrain.Runtime->LoadedMeshResolution = 192;
		auto& directionalLight = entity.AddComponent<gl::DirectionalLightComponent>();
		directionalLight.CastShadows = true;
		directionalLight.ShadowMapResolution = 4096;
		directionalLight.ShadowDistance = 135.0f;
		directionalLight.ShadowBias = 0.0025f;
		directionalLight.ShadowCascadeCount = 3;
		directionalLight.ShadowSplitLambda = 0.72f;
		directionalLight.ShadowCascadeBlend = 0.18f;

		gl::SceneSerializer(source).Serialize(path.string());
		context.Check(std::filesystem::is_regular_file(path), "minimal scene is written");

		gl::Ref<gl::Scene> restoredScene = gl::CreateRef<gl::Scene>();
		context.Check(gl::SceneSerializer(restoredScene).Deserialize(path.string()),
			"minimal scene reloads without a window or renderer");
		gl::Entity restored = restoredScene->FindEntityByUUID(entityUUID);
		context.Check(static_cast<bool>(restored), "stable UUID is restored into the scene index");
		if (!restored)
			return;

		context.Check(restored.GetComponent<gl::TagComponent>().Tag == "Regression Entity",
			"entity tag survives scene round trip");
		const auto& restoredTransform = restored.GetComponent<gl::TransformComponent>();
		context.Check(Near(restoredTransform.Translation, transform.Translation)
			&& Near(restoredTransform.Rotation, transform.Rotation)
			&& Near(restoredTransform.Scale, transform.Scale),
			"transform survives scene round trip");
		context.Check(restored.HasComponent<gl::ModelRendererComponent>()
			&& static_cast<uint64_t>(restored.GetComponent<gl::ModelRendererComponent>().ModelHandle) == 5001,
			"model handle survives scene round trip");
		context.Check(restored.HasComponent<gl::MaterialComponent>(),
			"material component survives scene round trip");
		if (!restored.HasComponent<gl::MaterialComponent>())
			return;

		const auto& restoredMaterial = restored.GetComponent<gl::MaterialComponent>();
		context.Check(static_cast<uint64_t>(restoredMaterial.MaterialHandle) == 5002
			&& restoredMaterial.Overrides.Mask == material.Overrides.Mask,
			"material handle and override mask survive scene round trip");
		context.Check(static_cast<uint64_t>(restoredMaterial.Overrides.Values.NormalTexture) == 5003
			&& static_cast<uint64_t>(restoredMaterial.Overrides.Values.AOTexture) == 5004
			&& static_cast<uint64_t>(restoredMaterial.Overrides.Values.EmissiveTexture) == 5005
			&& Near(restoredMaterial.Overrides.Values.AOStrength, 0.6f)
			&& Near(restoredMaterial.Overrides.Values.EmissiveColor, glm::vec3(0.8f, 0.3f, 0.1f))
			&& Near(restoredMaterial.Overrides.Values.EmissiveStrength, 3.0f),
			"material channel overrides survive scene round trip");
		context.Check(restored.HasComponent<gl::TerrainComponent>(),
			"terrain component survives scene round trip");
		if (restored.HasComponent<gl::TerrainComponent>())
		{
			const auto& restoredTerrain = restored.GetComponent<gl::TerrainComponent>();
			context.Check(SameTerrainSpecification(
				restoredTerrain.Specification, terrain.Specification),
				"terrain specification survives scene round trip");
			context.Check(!restoredTerrain.Runtime,
				"terrain runtime is not serialized");
		}
		context.Check(restored.HasComponent<gl::DirectionalLightComponent>(),
			"directional light component survives scene round trip");
		if (restored.HasComponent<gl::DirectionalLightComponent>())
		{
			const auto& restoredLight =
				restored.GetComponent<gl::DirectionalLightComponent>();
			context.Check(restoredLight.CastShadows
				&& restoredLight.ShadowMapResolution == 4096
				&& Near(restoredLight.ShadowDistance, 135.0f)
				&& Near(restoredLight.ShadowBias, 0.0025f)
				&& restoredLight.ShadowCascadeCount == 3
				&& Near(restoredLight.ShadowSplitLambda, 0.72f)
				&& Near(restoredLight.ShadowCascadeBlend, 0.18f),
				"directional shadow settings survive scene round trip");
		}
	}

	void TestTerrainCopyAndTransactions(TestContext& context)
	{
		gl::Ref<gl::Scene> source = gl::CreateRef<gl::Scene>();
		gl::Entity terrainEntity = source->CreateEntity("Terrain Lifecycle");
		terrainEntity.GetComponent<gl::TransformComponent>().Translation =
			{ 12.0f, 3.0f, -8.0f };
		auto& terrain = terrainEntity.AddComponent<gl::TerrainComponent>();
		terrain.Specification.Noise.Seed = 19;
		terrain.Specification.HeightScale = 28.0f;
		terrain.Runtime = gl::CreateRef<gl::TerrainRuntime>();
		terrain.Runtime->LoadedMeshResolution = 128;

		gl::Entity duplicate = source->DuplicateEntity(terrainEntity);
		auto& editorTerrain =
			terrainEntity.GetComponent<gl::TerrainComponent>();
		context.Check(duplicate.HasComponent<gl::TerrainComponent>()
			&& !duplicate.GetComponent<gl::TerrainComponent>().Runtime,
			"duplicating terrain copies specification without runtime state");
		context.Check(Near(
			duplicate.GetComponent<gl::TransformComponent>().Translation,
			terrainEntity.GetComponent<gl::TransformComponent>().Translation),
			"duplicating terrain preserves transform");

		gl::Ref<gl::Scene> runtimeScene = gl::Scene::Copy(source);
		gl::Entity runtimeTerrain = runtimeScene->FindEntityByUUID(
			terrainEntity.GetUUID());
		context.Check(runtimeTerrain
			&& runtimeTerrain.HasComponent<gl::TerrainComponent>()
			&& !runtimeTerrain.GetComponent<gl::TerrainComponent>().Runtime,
			"Edit to Play scene copy rebuilds terrain runtime independently");
		if (runtimeTerrain)
		{
			runtimeTerrain.GetComponent<gl::TerrainComponent>()
				.Specification.HeightScale = 99.0f;
			context.Check(Near(editorTerrain.Specification.HeightScale, 28.0f),
				"runtime terrain edits do not contaminate the editor scene");
		}

		gl::TerrainComponent assigned;
		assigned.Runtime = gl::CreateRef<gl::TerrainRuntime>();
		assigned = editorTerrain;
		context.Check(!assigned.Runtime
			&& SameTerrainSpecification(
				assigned.Specification, editorTerrain.Specification),
			"terrain copy assignment invalidates runtime ownership");

		gl::EditorCommandHistory history;
		const gl::TerrainComponent before = editorTerrain;
		gl::TerrainComponent after = editorTerrain;
		after.Specification.HeightScale = 46.0f;
		after.Specification.Noise.Frequency = 2.75f;
		const gl::UUID uuid = terrainEntity.GetUUID();
		auto apply = [source, uuid](const gl::TerrainComponent& value) {
			gl::Entity target = source->FindEntityByUUID(uuid);
			if (!target || !target.HasComponent<gl::TerrainComponent>())
				return false;
			target.GetComponent<gl::TerrainComponent>() = value;
			return true;
		};
		history.Execute(std::make_unique<gl::ValueEditorCommand<gl::TerrainComponent>>(
			"Edit Terrain Noise", before, after, apply));
		context.Check(Near(editorTerrain.Specification.HeightScale, 46.0f)
			&& Near(editorTerrain.Specification.Noise.Frequency, 2.75f)
			&& !editorTerrain.Runtime,
			"terrain edit command applies specification and invalidates runtime");
		context.Check(history.Undo()
			&& Near(editorTerrain.Specification.HeightScale, 28.0f)
			&& Near(editorTerrain.Specification.Noise.Frequency,
				before.Specification.Noise.Frequency),
			"terrain edit command restores the activation snapshot");
		context.Check(!history.Undo(),
			"one continuous terrain edit produces exactly one undo command");
		context.Check(history.Redo()
			&& Near(editorTerrain.Specification.HeightScale, 46.0f),
			"terrain edit command supports redo");
	}

	void TestTerrainPresets(TestContext& context)
	{
		context.Check(gl::TerrainRenderer::GetSamplingMode()
			== gl::TerrainRenderer::SamplingMode::FullFourLayers
			&& gl::TerrainRenderer::Statistics{}.Mode
				== gl::TerrainRenderer::SamplingMode::FullFourLayers,
			"terrain material sampling defaults to full four-layer quality");

		const gl::TerrainPreset presets[] = {
			gl::TerrainPreset::Alpine,
			gl::TerrainPreset::Plateau,
			gl::TerrainPreset::RollingHills,
			gl::TerrainPreset::Volcanic,
			gl::TerrainPreset::ErodedValley
		};
		int previousSeed = 0;
		for (gl::TerrainPreset preset : presets)
		{
			gl::TerrainSpecification first;
			gl::TerrainSpecification second;
			gl::ApplyTerrainPreset(first, preset);
			gl::ApplyTerrainPreset(second, preset);
			context.Check(SameTerrainSpecification(first, second),
				std::string("terrain preset is deterministic: ")
				+ gl::TerrainPresetToString(preset));
			context.Check(first.Preset == preset
				&& first.Noise.Seed != previousSeed
				&& first.Noise.GeologyBlend >= 0.0f
				&& first.Noise.GeologyBlend <= 1.0f
				&& first.Noise.GeologyScale >= 0.25f
				&& first.Noise.GeologyScale <= 12.0f
				&& first.Noise.RiftStrength >= 0.0f
				&& first.Noise.RiftStrength <= 0.5f
				&& first.Noise.TrendStrength >= 0.0f
				&& first.Noise.TrendStrength <= 0.5f
				&& first.Authoring.ThermalIterations <= 128
				&& first.Authoring.ThermalStrength >= 0.0f
				&& first.Authoring.ThermalStrength <= 0.5f,
				std::string("terrain preset has bounded authoring settings: ")
				+ gl::TerrainPresetToString(preset));
			previousSeed = first.Noise.Seed;
		}
		context.Check(gl::TerrainPresetFromString("unknown")
			== gl::TerrainPreset::Custom,
			"unknown terrain preset falls back to Custom");
	}

	void TestTerrainChunkLayout(TestContext& context)
	{
		const uint32_t sharedResolution =
			gl::TerrainChunkLayout::CalculateSharedMeshResolution(256);
		const auto chunks =
			gl::TerrainChunkLayout::Build(256.0f, sharedResolution);
		context.Check(sharedResolution == 86
			&& chunks.size() == gl::TerrainChunkLayout::ChunkCount,
			"terrain chunks share one ceil-divided mesh resolution");

		bool regionsAreContinuous = true;
		for (uint32_t z = 0; z < gl::TerrainChunkLayout::AxisCount; ++z)
		{
			for (uint32_t x = 0; x < gl::TerrainChunkLayout::AxisCount; ++x)
			{
				const auto& chunk =
					chunks[z * gl::TerrainChunkLayout::AxisCount + x];
				regionsAreContinuous = regionsAreContinuous
					&& Near(chunk.UVScale, glm::vec2(1.0f / 3.0f))
					&& Near(
						chunk.LocalScale
							* static_cast<float>(sharedResolution),
						chunk.WorldSize);
				if (x + 1 < gl::TerrainChunkLayout::AxisCount)
				{
					const auto& right = chunks[
						z * gl::TerrainChunkLayout::AxisCount + x + 1];
					regionsAreContinuous = regionsAreContinuous
						&& Near(chunk.UVOffset.x + chunk.UVScale.x,
							right.UVOffset.x)
						&& Near(chunk.LocalOffset.x
								+ chunk.WorldSize * 0.5f,
							right.LocalOffset.x
								- right.WorldSize * 0.5f);
				}
				if (z + 1 < gl::TerrainChunkLayout::AxisCount)
				{
					const auto& above = chunks[
						(z + 1) * gl::TerrainChunkLayout::AxisCount + x];
					regionsAreContinuous = regionsAreContinuous
						&& Near(chunk.UVOffset.y + chunk.UVScale.y,
							above.UVOffset.y)
						&& Near(chunk.LocalOffset.y
								+ chunk.WorldSize * 0.5f,
							above.LocalOffset.y
								- above.WorldSize * 0.5f);
				}
			}
		}
		const auto& first = chunks.front();
		const auto& last = chunks.back();
		regionsAreContinuous = regionsAreContinuous
			&& Near(first.LocalOffset.x - first.WorldSize * 0.5f, -128.0f)
			&& Near(first.LocalOffset.y - first.WorldSize * 0.5f, -128.0f)
			&& Near(last.LocalOffset.x + last.WorldSize * 0.5f, 128.0f)
			&& Near(last.LocalOffset.y + last.WorldSize * 0.5f, 128.0f)
			&& Near(last.UVOffset + last.UVScale, glm::vec2(1.0f));
		context.Check(regionsAreContinuous,
			"3x3 terrain chunks cover continuous UV and local-space bounds");

		const auto lodResolutions =
			gl::TerrainChunkLayout::CalculateLODResolutions(sharedResolution);
		context.Check(lodResolutions[0] == 86
			&& lodResolutions[1] == 43 && lodResolutions[2] == 22,
			"terrain chunk LOD meshes reduce shared resolution by powers of two");
		context.Check(gl::TerrainChunkLayout::SelectLODLevel(89.0f, 90.0f, 180.0f) == 0
			&& gl::TerrainChunkLayout::SelectLODLevel(90.0f, 90.0f, 180.0f) == 1
			&& gl::TerrainChunkLayout::SelectLODLevel(180.0f, 90.0f, 180.0f) == 2,
			"terrain chunk LOD selection follows near middle and far thresholds");
		context.Check(gl::TerrainChunkLayout::SelectLODLevelWithHysteresis(
			92.0f, 90.0f, 180.0f, 0, 5.0f) == 0
			&& gl::TerrainChunkLayout::SelectLODLevelWithHysteresis(
				96.0f, 90.0f, 180.0f, 0, 5.0f) == 1
			&& gl::TerrainChunkLayout::SelectLODLevelWithHysteresis(
				87.0f, 90.0f, 180.0f, 1, 5.0f) == 1,
			"terrain chunk LOD hysteresis prevents threshold flicker");
		std::array<uint32_t, gl::TerrainChunkLayout::ChunkCount> unstable{
			0, 2, 2,
			2, 2, 2,
			2, 2, 2
		};
		const auto stable =
			gl::TerrainChunkLayout::StabilizeNeighborLODs(unstable);
		bool neighborDeltaIsBounded = true;
		for (uint32_t z = 0; z < gl::TerrainChunkLayout::AxisCount; ++z)
			for (uint32_t x = 0; x < gl::TerrainChunkLayout::AxisCount; ++x)
			{
				const uint32_t index = z * gl::TerrainChunkLayout::AxisCount + x;
				if (x + 1 < gl::TerrainChunkLayout::AxisCount)
					neighborDeltaIsBounded &= std::abs(
						static_cast<int>(stable[index])
						- static_cast<int>(stable[index + 1])) <= 1;
				if (z + 1 < gl::TerrainChunkLayout::AxisCount)
					neighborDeltaIsBounded &= std::abs(
						static_cast<int>(stable[index])
						- static_cast<int>(stable[index
							+ gl::TerrainChunkLayout::AxisCount])) <= 1;
			}
		context.Check(neighborDeltaIsBounded && stable[1] == 1 && stable[3] == 1,
			"terrain chunk LOD stabilization limits adjacent chunks to one level");
	}

	void TestTerrainHydrologyRuntime(TestContext& context)
	{
		gl::TerrainHydrologySpecification specification;
		specification.Width = 3;
		specification.Height = 1;
		specification.CellSize = 1.0f;
		specification.FixedTimeStep = 0.01f;
		specification.MaxSubsteps = 4;
		specification.FluxDamping = 0.98f;

		gl::TerrainHydrologyRuntime pausedRuntime(
			specification, { 0.0f, 1.0f, 0.0f });
		pausedRuntime.SetWaterDepth({ 0.0f, 1.0f, 0.0f });
		pausedRuntime.SetSedimentDensity({ 0.0f, 1.0f, 0.0f });
		context.Check(pausedRuntime.Advance(0.2f) == 0
			&& pausedRuntime.GetStatistics().StepCount == 0,
			"paused hydrology does not advance with editor frame time");
		context.Check(pausedRuntime.SingleStep()
			&& pausedRuntime.GetStatistics().StepCount == 1
			&& pausedRuntime.GetState().Water[1] < 1.0f
			&& pausedRuntime.GetState().Water[0] > 0.0f
			&& pausedRuntime.GetState().Water[2] > 0.0f,
			"single step moves water from higher surface to lower neighbors");
		context.Check(pausedRuntime.GetState().Sediment[1] < 1.0f
			&& pausedRuntime.GetState().Sediment[0] > 0.0f
			&& pausedRuntime.GetState().Sediment[2] > 0.0f,
			"suspended sediment follows water flux toward downstream cells");
		context.Check(pausedRuntime.GetStatistics().MaximumSedimentCapacity > 0.0f
			&& pausedRuntime.GetStatistics().MaximumSedimentSaturation <= 1000.0f,
			"sediment capacity and saturation are finite derived diagnostics");
		const auto& singleStepStats = pausedRuntime.GetStatistics();
		context.Check(singleStepStats.Finite
			&& singleStepStats.MinimumWaterDepth >= 0.0f
			&& std::abs(singleStepStats.MassError) < 1.0e-5,
			"closed hydrology step remains finite non-negative and conservative");
		pausedRuntime.Reset();
		context.Check(pausedRuntime.GetStatistics().StepCount == 0
			&& Near(pausedRuntime.GetState().Water[1], 1.0f)
			&& Near(pausedRuntime.GetState().Water[0], 0.0f)
			&& Near(pausedRuntime.GetState().Sediment[1], 1.0f)
			&& Near(pausedRuntime.GetState().Sediment[0], 0.0f),
			"hydrology reset restores initial water and sediment snapshots");
		pausedRuntime.SetSedimentCapacityScale(0.0f);
		context.Check(Near(
			pausedRuntime.GetStatistics().MaximumSedimentCapacity, 0.0f)
			&& Near(pausedRuntime.GetStatistics().MaximumSedimentSaturation, 1000.0f),
			"zero capacity scale reports bounded oversaturation without changing mass");

		gl::TerrainHydrologyRuntime largeFrames(
			specification, { 0.0f, 1.0f, 0.0f });
		gl::TerrainHydrologyRuntime smallFrames(
			specification, { 0.0f, 1.0f, 0.0f });
		largeFrames.SetWaterDepth({ 0.0f, 1.0f, 0.0f });
		smallFrames.SetWaterDepth({ 0.0f, 1.0f, 0.0f });
		largeFrames.SetSedimentDensity({ 0.0f, 1.0f, 0.0f });
		smallFrames.SetSedimentDensity({ 0.0f, 1.0f, 0.0f });
		largeFrames.Play();
		smallFrames.Play();
		for (uint32_t frame = 0; frame < 25; ++frame)
			largeFrames.Advance(0.04f);
		for (uint32_t frame = 0; frame < 100; ++frame)
			smallFrames.Advance(0.01f);
		bool partitionIndependent =
			largeFrames.GetStatistics().StepCount
				== smallFrames.GetStatistics().StepCount;
		for (size_t index = 0;
			index < largeFrames.GetState().Water.size(); ++index)
		{
			partitionIndependent &= Near(
				largeFrames.GetState().Water[index],
				smallFrames.GetState().Water[index], 1.0e-6f)
				&& Near(largeFrames.GetState().Velocity[index],
					smallFrames.GetState().Velocity[index])
				&& Near(largeFrames.GetState().Sediment[index],
					smallFrames.GetState().Sediment[index], 1.0e-6f)
				&& Near(largeFrames.GetState().SedimentCapacity[index],
					smallFrames.GetState().SedimentCapacity[index], 1.0e-6f)
				&& Near(largeFrames.GetState().SedimentSaturation[index],
					smallFrames.GetState().SedimentSaturation[index], 1.0e-5f);
		}
		context.Check(partitionIndependent,
			"fixed water and sediment results are independent of frame partitioning");
		const auto& sedimentStats = largeFrames.GetStatistics();
		context.Check(sedimentStats.Finite
			&& sedimentStats.MinimumSediment >= 0.0f
			&& std::abs(sedimentStats.SedimentMassError) < 1.0e-5
			&& Near(static_cast<float>(sedimentStats.SedimentBoundaryLoss), 0.0f),
			"closed sediment transport remains finite non-negative and conservative");
		context.Check(largeFrames.GetState().Height
			== std::vector<float>({ 0.0f, 1.0f, 0.0f }),
			"transport-only sediment does not modify terrain height");

		gl::TerrainHydrologySpecification erosionSpecification;
		erosionSpecification.Width = 2;
		erosionSpecification.Height = 1;
		erosionSpecification.CellSize = 1.0f;
		erosionSpecification.FixedTimeStep = 0.01f;
		erosionSpecification.MaxSubsteps = 4;
		erosionSpecification.FluxDamping = 0.0f;
		erosionSpecification.SedimentCapacityScale = 100.0f;
		erosionSpecification.ErosionRate = 10.0f;
		erosionSpecification.DepositionRate = 0.0f;
		erosionSpecification.TerrainDensity = 2.0f;
		erosionSpecification.MaximumErosionDepth = 0.005f;
		erosionSpecification.MaximumHeightChangePerStep = 0.002f;
		gl::TerrainHydrologyRuntime erosionRuntime(
			erosionSpecification, { 1.0f, 0.0f });
		erosionRuntime.SetWaterDepth({ 1.0f, 0.0f });
		context.Check(erosionRuntime.SingleStep()
			&& erosionRuntime.GetState().Height[0] < 1.0f
			&& erosionRuntime.GetState().Sediment[0] > 0.0f
			&& erosionRuntime.GetStatistics().CumulativeErodedMass > 0.0
			&& erosionRuntime.GetStatistics().MaximumAbsoluteHeightChangePerStep
				<= erosionSpecification.MaximumHeightChangePerStep + 1.0e-6f,
			"undersaturated moving water erodes terrain within the per-step limit");
		for (uint32_t step = 0; step < 20; ++step)
			erosionRuntime.SingleStep();
		context.Check(erosionRuntime.GetState().Height[0]
				>= 1.0f - erosionSpecification.MaximumErosionDepth - 1.0e-6f
			&& std::abs(
				erosionRuntime.GetStatistics().TerrainSedimentMassError) < 1.0e-5,
			"runtime erosion respects the erodible floor and combined mass budget");
		erosionRuntime.Reset();
		context.Check(Near(erosionRuntime.GetState().Height[0], 1.0f)
			&& Near(erosionRuntime.GetState().Sediment[0], 0.0f)
			&& Near(static_cast<float>(
				erosionRuntime.GetStatistics().CumulativeErodedMass), 0.0f),
			"hydrology reset restores pre-erosion terrain and sediment");

		gl::TerrainHydrologySpecification depositionSpecification;
		depositionSpecification.Width = 1;
		depositionSpecification.Height = 1;
		depositionSpecification.CellSize = 1.0f;
		depositionSpecification.FixedTimeStep = 0.01f;
		depositionSpecification.ErosionRate = 0.0f;
		depositionSpecification.DepositionRate = 10.0f;
		depositionSpecification.TerrainDensity = 2.0f;
		depositionSpecification.MaximumHeightChangePerStep = 0.002f;
		gl::TerrainHydrologyRuntime depositionRuntime(
			depositionSpecification, { 0.0f });
		depositionRuntime.SetWaterDepth({ 1.0f });
		depositionRuntime.SetSedimentDensity({ 1.0f });
		context.Check(depositionRuntime.SingleStep()
			&& depositionRuntime.GetState().Height[0] > 0.0f
			&& depositionRuntime.GetState().Sediment[0] < 1.0f
			&& depositionRuntime.GetStatistics().CumulativeDepositedMass > 0.0
			&& std::abs(
				depositionRuntime.GetStatistics().TerrainSedimentMassError) < 1.0e-5,
			"oversaturated water deposits sediment within the combined mass budget");

		gl::TerrainHydrologyRuntime erosionLargeFrames(
			erosionSpecification, { 1.0f, 0.0f });
		gl::TerrainHydrologyRuntime erosionSmallFrames(
			erosionSpecification, { 1.0f, 0.0f });
		erosionLargeFrames.SetWaterDepth({ 1.0f, 0.0f });
		erosionSmallFrames.SetWaterDepth({ 1.0f, 0.0f });
		erosionLargeFrames.Play();
		erosionSmallFrames.Play();
		for (uint32_t frame = 0; frame < 25; ++frame)
			erosionLargeFrames.Advance(0.04f);
		for (uint32_t frame = 0; frame < 100; ++frame)
			erosionSmallFrames.Advance(0.01f);
		bool erosionPartitionIndependent =
			erosionLargeFrames.GetStatistics().StepCount
				== erosionSmallFrames.GetStatistics().StepCount;
		for (size_t index = 0;
			index < erosionLargeFrames.GetState().Height.size(); ++index)
		{
			erosionPartitionIndependent &= Near(
				erosionLargeFrames.GetState().Height[index],
				erosionSmallFrames.GetState().Height[index], 1.0e-6f)
				&& Near(erosionLargeFrames.GetState().Sediment[index],
					erosionSmallFrames.GetState().Sediment[index], 1.0e-6f);
		}
		context.Check(erosionPartitionIndependent,
			"runtime erosion and deposition are independent of frame partitioning");

		gl::TerrainHydrologySpecification catchUpSpecification = specification;
		catchUpSpecification.MaxSubsteps = 2;
		gl::TerrainHydrologyRuntime catchUpRuntime(
			catchUpSpecification, { 0.0f, 0.0f, 0.0f });
		catchUpRuntime.Play();
		context.Check(catchUpRuntime.Advance(0.10f) == 2
			&& catchUpRuntime.GetStatistics().DroppedTime > 0.07,
			"fixed hydrology caps catch-up work and reports dropped time");

		gl::TerrainHydrologySpecification rainSpecification = specification;
		rainSpecification.RainfallRate = 0.2f;
		gl::TerrainHydrologyRuntime basinRuntime(
			rainSpecification, { 1.0f, 0.0f, 1.0f });
		basinRuntime.Play();
		for (uint32_t frame = 0; frame < 100; ++frame)
			basinRuntime.Advance(0.01f);
		const auto& basinWater = basinRuntime.GetState().Water;
		const auto& basinStats = basinRuntime.GetStatistics();
		context.Check(basinWater[1] > basinWater[0]
			&& basinWater[1] > basinWater[2],
			"lower terrain cell accumulates rainfall as a basin");
		context.Check(basinStats.Finite
			&& basinStats.MinimumWaterDepth >= 0.0f
			&& std::abs(basinStats.MassError) < 1.0e-4,
			"rainfall volume is included in hydrology mass accounting");
	}

	void TestTerrainClimateRuntime(TestContext& context)
	{
		gl::TerrainClimateSpecification transportSpecification;
		transportSpecification.Width = 3;
		transportSpecification.Height = 1;
		transportSpecification.CellSize = 1.0f;
		transportSpecification.FixedTimeStep = 1.0f;
		transportSpecification.MaxSubsteps = 4;
		transportSpecification.WindVelocity = { 1.0f, 0.0f };
		transportSpecification.TemperatureRelaxationRate = 0.0f;
		transportSpecification.EvaporationRate = 0.0f;
		transportSpecification.CondensationRate = 0.0f;
		transportSpecification.OrographicRainRate = 0.0f;
		transportSpecification.VegetationResponseRate = 0.0f;

		gl::TerrainClimateRuntime transportRuntime(
			transportSpecification, { 0.0f, 0.0f, 0.0f });
		transportRuntime.SetAtmosphericMoisture({ 1.0f, 0.0f, 0.0f });
		context.Check(transportRuntime.Advance(1.0f) == 0
			&& transportRuntime.GetStatistics().StepCount == 0,
			"paused climate does not advance with editor frame time");
		context.Check(transportRuntime.SingleStep()
			&& Near(transportRuntime.GetState().AtmosphericMoisture[0], 0.0f)
			&& Near(transportRuntime.GetState().AtmosphericMoisture[1], 1.0f)
			&& Near(transportRuntime.GetState().AtmosphericMoisture[2], 0.0f),
			"conservative upwind transport follows the configured wind direction");
		context.Check(transportRuntime.GetStatistics().Finite
			&& std::abs(transportRuntime.GetStatistics().WaterBudgetError)
				< 1.0e-6,
			"closed climate transport remains finite and conserves total water");
		transportRuntime.Reset();
		context.Check(transportRuntime.GetStatistics().StepCount == 0
			&& Near(transportRuntime.GetState().AtmosphericMoisture[0], 1.0f)
			&& Near(transportRuntime.GetState().AtmosphericMoisture[1], 0.0f),
			"climate reset restores the configured initial fields");

		gl::TerrainClimateSpecification evaporationSpecification =
			transportSpecification;
		evaporationSpecification.Width = 1;
		evaporationSpecification.WindVelocity = { 0.0f, 0.0f };
		evaporationSpecification.EvaporationRate = 0.1f;
		gl::TerrainClimateRuntime evaporationRuntime(
			evaporationSpecification, { 0.0f });
		evaporationRuntime.SetSurfaceWater({ 0.2f });
		context.Check(evaporationRuntime.SingleStep()
			&& Near(evaporationRuntime.GetState().SurfaceWater[0], 0.1f)
			&& Near(
				evaporationRuntime.GetState().AtmosphericMoisture[0], 0.1f)
			&& Near(static_cast<float>(
				evaporationRuntime.GetStatistics().CumulativeEvaporationVolume),
				0.1f),
			"evaporation transfers water from the surface into atmospheric moisture");

		gl::TerrainClimateSpecification rainSpecification =
			transportSpecification;
		rainSpecification.SaturationMoistureDepth = 10.0f;
		rainSpecification.OrographicRainRate = 1.0f;
		gl::TerrainClimateRuntime risingTerrain(
			rainSpecification, { 0.0f, 1.0f, 2.0f });
		gl::TerrainClimateRuntime flatTerrain(
			rainSpecification, { 0.0f, 0.0f, 0.0f });
		risingTerrain.SetAtmosphericMoisture({ 0.1f, 0.1f, 0.1f });
		flatTerrain.SetAtmosphericMoisture({ 0.1f, 0.1f, 0.1f });
		risingTerrain.SingleStep();
		flatTerrain.SingleStep();
		context.Check(risingTerrain.GetStatistics().MaximumRainfall > 0.0f
			&& Near(flatTerrain.GetStatistics().MaximumRainfall, 0.0f),
			"windward terrain lift produces rain while flat terrain does not");
		context.Check(std::abs(
			risingTerrain.GetStatistics().WaterBudgetError) < 1.0e-6,
			"orographic rainfall transfers atmospheric water without creating mass");

		gl::TerrainClimateSpecification vegetationSpecification =
			transportSpecification;
		vegetationSpecification.Width = 2;
		vegetationSpecification.WindVelocity = { 0.0f, 0.0f };
		vegetationSpecification.VegetationResponseRate = 1.0f;
		gl::TerrainClimateRuntime vegetationRuntime(
			vegetationSpecification, { 0.0f, 0.0f });
		vegetationRuntime.SetTemperature({ 18.0f, 18.0f });
		vegetationRuntime.SetSurfaceWater({ 0.02f, 0.0f });
		context.Check(vegetationRuntime.SingleStep()
			&& Near(vegetationRuntime.GetState().VegetationPotential[0], 1.0f)
			&& Near(vegetationRuntime.GetState().VegetationPotential[1], 0.0f),
			"vegetation potential responds to local water and temperature");

		gl::TerrainClimateSpecification fixedStepSpecification =
			transportSpecification;
		fixedStepSpecification.FixedTimeStep = 0.25f;
		fixedStepSpecification.WindVelocity = { 0.5f, 0.0f };
		fixedStepSpecification.EvaporationRate = 0.01f;
		fixedStepSpecification.CondensationRate = 0.5f;
		fixedStepSpecification.OrographicRainRate = 0.1f;
		fixedStepSpecification.VegetationResponseRate = 0.5f;
		gl::TerrainClimateRuntime largeFrameRuntime(
			fixedStepSpecification, { 0.0f, 1.0f, 0.0f });
		gl::TerrainClimateRuntime smallFrameRuntime(
			fixedStepSpecification, { 0.0f, 1.0f, 0.0f });
		largeFrameRuntime.SetAtmosphericMoisture({ 0.03f, 0.01f, 0.0f });
		smallFrameRuntime.SetAtmosphericMoisture({ 0.03f, 0.01f, 0.0f });
		largeFrameRuntime.SetSurfaceWater({ 0.02f, 0.01f, 0.0f });
		smallFrameRuntime.SetSurfaceWater({ 0.02f, 0.01f, 0.0f });
		largeFrameRuntime.Play();
		smallFrameRuntime.Play();
		largeFrameRuntime.Advance(1.0f);
		for (uint32_t frame = 0; frame < 4; ++frame)
			smallFrameRuntime.Advance(0.25f);
		bool partitionIndependent =
			largeFrameRuntime.GetStatistics().StepCount
				== smallFrameRuntime.GetStatistics().StepCount;
		for (size_t index = 0;
			index < largeFrameRuntime.GetState().Temperature.size(); ++index)
		{
			partitionIndependent &= Near(
				largeFrameRuntime.GetState().Temperature[index],
				smallFrameRuntime.GetState().Temperature[index], 1.0e-6f)
				&& Near(
					largeFrameRuntime.GetState().AtmosphericMoisture[index],
					smallFrameRuntime.GetState().AtmosphericMoisture[index],
					1.0e-6f)
				&& Near(largeFrameRuntime.GetState().SurfaceWater[index],
					smallFrameRuntime.GetState().SurfaceWater[index], 1.0e-6f)
				&& Near(
					largeFrameRuntime.GetState().VegetationPotential[index],
					smallFrameRuntime.GetState().VegetationPotential[index],
					1.0e-6f);
		}
		context.Check(partitionIndependent,
			"fixed climate results are independent of frame partitioning");
	}

	void TestShadowFrustumCulling(TestContext& context)
	{
		const glm::mat4 identity(1.0f);
		context.Check(gl::ShadowRenderer::IntersectsClipFrustum(
			{ -0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, 0.5f }, identity, identity),
			"shadow frustum keeps bounds fully inside clip space");
		context.Check(!gl::ShadowRenderer::IntersectsClipFrustum(
			{ 2.0f, -0.5f, -0.5f }, { 3.0f, 0.5f, 0.5f }, identity, identity),
			"shadow frustum culls bounds fully outside one clip plane");
		context.Check(gl::ShadowRenderer::IntersectsClipFrustum(
			{ 0.5f, -0.5f, -0.5f }, { 1.5f, 0.5f, 0.5f }, identity, identity),
			"shadow frustum conservatively keeps bounds crossing a clip plane");
		context.Check(!gl::ShadowRenderer::IntersectsClipFrustum(
			{ -0.25f, -0.25f, -0.25f }, { 0.25f, 0.25f, 0.25f },
			glm::translate(identity, glm::vec3(0.0f, 0.0f, 3.0f)), identity),
			"shadow frustum applies entity transform before culling");

		gl::ShadowRenderer::Statistics batchedStatistics;
		batchedStatistics.RenderedDraws = 24;
		batchedStatistics.DrawCalls = 4;
		context.Check(batchedStatistics.GetSavedDrawCalls() == 20,
			"shadow statistics report draw calls saved by instancing");
		batchedStatistics.DrawCalls = 25;
		context.Check(batchedStatistics.GetSavedDrawCalls() == 0,
			"shadow saved draw count cannot underflow");
		context.Check(gl::ShadowRenderer::ShouldCastShadow(
			gl::MaterialAlphaMode::Opaque),
			"opaque materials cast directional shadows");
		context.Check(gl::ShadowRenderer::ShouldCastShadow(
			gl::MaterialAlphaMode::Mask),
			"mask materials cast directional shadows");
		context.Check(!gl::ShadowRenderer::ShouldCastShadow(
			gl::MaterialAlphaMode::Blend),
			"blend materials do not cast solid directional shadows");
	}

	void TestTerrainCameraFrustumCulling(TestContext& context)
	{
		const glm::mat4 identity(1.0f);
		context.Check(gl::TerrainRenderer::IntersectsCameraFrustum(
			{ -0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, 0.5f }, identity, identity),
			"terrain camera frustum keeps a chunk fully inside clip space");
		context.Check(!gl::TerrainRenderer::IntersectsCameraFrustum(
			{ 2.0f, -0.5f, -0.5f }, { 3.0f, 0.5f, 0.5f }, identity, identity),
			"terrain camera frustum culls a chunk fully outside one clip plane");
		context.Check(gl::TerrainRenderer::IntersectsCameraFrustum(
			{ 0.5f, -0.5f, -0.5f }, { 1.5f, 0.5f, 0.5f }, identity, identity),
			"terrain camera frustum conservatively keeps a boundary-crossing chunk");
		context.Check(!gl::TerrainRenderer::IntersectsCameraFrustum(
			{ -0.25f, -0.25f, -0.25f }, { 0.25f, 0.25f, 0.25f },
			glm::translate(identity, glm::vec3(0.0f, 0.0f, 3.0f)), identity),
			"terrain camera frustum applies the terrain entity transform");
	}

	void TestEnvironmentMapFoundation(
		TestContext& context,
		const std::filesystem::path& root)
	{
		context.Check(gl::CalculateTextureMipCount(1) == 1
			&& gl::CalculateTextureMipCount(2) == 2
			&& gl::CalculateTextureMipCount(256) == 9,
			"cubemap mip count includes the complete 1x1 chain");

		const glm::vec3 expectedDirections[] = {
			{ 1.0f, 0.0f, 0.0f },
			{ -1.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f },
			{ 0.0f, -1.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f },
			{ 0.0f, 0.0f, -1.0f }
		};
		bool centersMatch = true;
		for (uint32_t face = 0; face < 6; ++face)
		{
			centersMatch = centersMatch && Near(
				gl::EnvironmentMapLoader::CubemapDirection(
					static_cast<gl::TextureCubeFace>(face), 0.0f, 0.0f),
				expectedDirections[face]);
		}
		context.Check(centersMatch,
			"equirectangular conversion follows the TextureCube face convention");

		gl::FloatImageData source;
		source.Width = 8;
		source.Height = 4;
		source.Pixels.resize(
			static_cast<size_t>(source.Width) * source.Height * 4);
		for (size_t offset = 0; offset < source.Pixels.size(); offset += 4)
		{
			source.Pixels[offset + 0] = 4.0f;
			source.Pixels[offset + 1] = 2.0f;
			source.Pixels[offset + 2] = 0.5f;
			source.Pixels[offset + 3] = 1.0f;
		}
		context.Check(gl::EnvironmentMapLoader::SuggestFaceSize(source) == 2,
			"2:1 HDR source suggests a proportional cubemap face size");

		gl::CubemapFloatData converted;
		const bool convertedSuccessfully =
			gl::EnvironmentMapLoader::ConvertEquirectangularToCubemap(
				source, 3, converted);
		bool preservesHDR = convertedSuccessfully && converted.IsValid();
		for (const auto& face : converted.Faces)
		{
			for (size_t offset = 0; preservesHDR && offset < face.size();
				offset += 4)
			{
				preservesHDR = Near(face[offset + 0], 4.0f)
					&& Near(face[offset + 1], 2.0f)
					&& Near(face[offset + 2], 0.5f)
					&& Near(face[offset + 3], 1.0f);
			}
		}
		context.Check(preservesHDR,
			"equirectangular conversion preserves values above display white");

		gl::CubemapFloatData constantEnvironment;
		constexpr float irradiancePi = 3.14159265358979323846f;
		constantEnvironment.Size = 4;
		for (auto& face : constantEnvironment.Faces)
		{
			face.resize(4 * 4 * 4);
			for (size_t offset = 0; offset < face.size(); offset += 4)
			{
				face[offset + 0] = 2.0f;
				face[offset + 1] = 0.5f;
				face[offset + 2] = 0.25f;
				face[offset + 3] = 1.0f;
			}
		}
		gl::CubemapFloatData diffuseIrradiance;
		const bool generatedIrradiance =
			gl::EnvironmentMapLoader::GenerateDiffuseIrradiance(
				constantEnvironment, 4, 64, diffuseIrradiance);
		bool constantIrradianceMatches =
			generatedIrradiance && diffuseIrradiance.IsValid();
		for (const auto& face : diffuseIrradiance.Faces)
		{
			for (size_t offset = 0;
				constantIrradianceMatches && offset < face.size();
				offset += 4)
			{
				constantIrradianceMatches =
					std::abs(face[offset + 0] - 2.0f * irradiancePi) < 0.002f
					&& std::abs(face[offset + 1] - 0.5f * irradiancePi) < 0.002f
					&& std::abs(face[offset + 2] - 0.25f * irradiancePi) < 0.002f
					&& Near(face[offset + 3], 1.0f);
			}
		}
		context.Check(constantIrradianceMatches,
			"cosine-weighted diffuse convolution integrates a constant environment");

		gl::CubemapMipChainFloatData constantPrefilter;
		const bool generatedConstantPrefilter =
			gl::EnvironmentMapLoader::GenerateSpecularPrefilter(
				constantEnvironment, 8, 64, constantPrefilter);
		bool constantPrefilterMatches =
			generatedConstantPrefilter && constantPrefilter.IsValid()
			&& constantPrefilter.Mips.size() == 4;
		for (const auto& mip : constantPrefilter.Mips)
		{
			for (const auto& face : mip.Faces)
			{
				for (size_t offset = 0;
					constantPrefilterMatches && offset < face.size();
					offset += 4)
				{
					constantPrefilterMatches =
						std::abs(face[offset + 0] - 2.0f) < 0.002f
						&& std::abs(face[offset + 1] - 0.5f) < 0.002f
						&& std::abs(face[offset + 2] - 0.25f) < 0.002f
						&& Near(face[offset + 3], 1.0f);
				}
			}
		}
		context.Check(constantPrefilterMatches,
			"GGX prefilter preserves constant radiance across its complete mip chain");

		gl::CubemapFloatData focusedEnvironment;
		focusedEnvironment.Size = 8;
		for (uint32_t faceIndex = 0;
			faceIndex < focusedEnvironment.Faces.size(); ++faceIndex)
		{
			auto& face = focusedEnvironment.Faces[faceIndex];
			face.resize(8 * 8 * 4);
			for (size_t offset = 0; offset < face.size(); offset += 4)
			{
				const float radiance = faceIndex == 0 ? 1.0f : 0.0f;
				face[offset + 0] = radiance;
				face[offset + 1] = radiance;
				face[offset + 2] = radiance;
				face[offset + 3] = 1.0f;
			}
		}
		gl::CubemapMipChainFloatData focusedPrefilter;
		const bool generatedFocusedPrefilter =
			gl::EnvironmentMapLoader::GenerateSpecularPrefilter(
				focusedEnvironment, 8, 128, focusedPrefilter);
		const auto averageRed = [](const gl::CubemapFloatData& mip)
		{
			const auto& face = mip.Faces[0];
			float total = 0.0f;
			for (size_t offset = 0; offset < face.size(); offset += 4)
				total += face[offset];
			return total / static_cast<float>(face.size() / 4);
		};
		const bool roughnessBlursFocusedRadiance =
			generatedFocusedPrefilter && focusedPrefilter.IsValid()
			&& averageRed(focusedPrefilter.Mips.front()) > 0.95f
			&& averageRed(focusedPrefilter.Mips.back()) > 0.0f
			&& averageRed(focusedPrefilter.Mips.back()) < 0.85f;
		context.Check(roughnessBlursFocusedRadiance,
			"higher prefilter mip levels broaden a focused environment reflection");

		gl::BrdfLutFloatData brdfLut;
		const bool generatedBrdfLut =
			gl::EnvironmentMapLoader::GenerateBrdfLut(16, 256, brdfLut);
		bool brdfLutIsFiniteAndBounded =
			generatedBrdfLut && brdfLut.IsValid();
		for (float value : brdfLut.Pixels)
		{
			brdfLutIsFiniteAndBounded =
				brdfLutIsFiniteAndBounded
				&& std::isfinite(value)
				&& value >= 0.0f
				&& value <= 1.05f;
		}
		context.Check(brdfLutIsFiniteAndBounded,
			"split-sum BRDF LUT is finite and bounded");
		const auto sampleBrdf = [&brdfLut](uint32_t x, uint32_t y)
		{
			const size_t offset =
				(static_cast<size_t>(y) * brdfLut.Size + x) * 2;
			return glm::vec2(
				brdfLut.Pixels[offset],
				brdfLut.Pixels[offset + 1]);
		};
		const glm::vec2 smoothFacing = sampleBrdf(15, 0);
		const glm::vec2 roughFacing = sampleBrdf(15, 15);
		const glm::vec2 grazing = sampleBrdf(0, 0);
		context.Check(
			smoothFacing.x > roughFacing.x
				&& grazing.y > smoothFacing.y,
			"BRDF LUT responds to roughness and grazing Fresnel");

		gl::EnvironmentDerivedMapKey cacheKey;
		cacheKey.SourceHandle = gl::AssetHandle(42);
		cacheKey.SourceVersion = 3;
		cacheKey.Type = gl::EnvironmentDerivedMapType::DiffuseIrradiance;
		cacheKey.Resolution = 32;
		cacheKey.SampleCount = 64;
		gl::EnvironmentDerivedMapKey sameCacheKey = cacheKey;
		gl::EnvironmentDerivedMapKey reloadedKey = cacheKey;
		++reloadedKey.SourceVersion;
		gl::EnvironmentDerivedMapKey parameterKey = cacheKey;
		parameterKey.SampleCount = 128;
		gl::EnvironmentDerivedMapKey specularKey = cacheKey;
		specularKey.Type = gl::EnvironmentDerivedMapType::SpecularPrefilter;
		const gl::EnvironmentDerivedMapKeyHash keyHash;
		context.Check(cacheKey == sameCacheKey
			&& keyHash(cacheKey) == keyHash(sameCacheKey),
			"identical environment source versions and parameters share a cache key");
		context.Check(!(cacheKey == reloadedKey)
			&& !(cacheKey == parameterKey)
			&& !(cacheKey == specularKey),
			"map type, environment reloads and settings isolate derived cache keys");

		gl::FloatImageData directionalSource;
		directionalSource.Width = 64;
		directionalSource.Height = 32;
		directionalSource.Pixels.resize(
			static_cast<size_t>(directionalSource.Width)
			* directionalSource.Height * 4);
		constexpr float pi = 3.14159265358979323846f;
		for (uint32_t y = 0; y < directionalSource.Height; ++y)
		{
			const float latitude = pi
				* (static_cast<float>(y) + 0.5f)
				/ static_cast<float>(directionalSource.Height);
			for (uint32_t x = 0; x < directionalSource.Width; ++x)
			{
				const float longitude = 2.0f * pi
					* (static_cast<float>(x) + 0.5f)
					/ static_cast<float>(directionalSource.Width) - pi;
				const glm::vec3 direction{
					std::sin(latitude) * std::cos(longitude),
					std::cos(latitude),
					std::sin(latitude) * std::sin(longitude)
				};
				const size_t offset =
					(static_cast<size_t>(y) * directionalSource.Width + x) * 4;
				directionalSource.Pixels[offset + 0] = direction.x * 0.5f + 0.5f;
				directionalSource.Pixels[offset + 1] = direction.y * 0.5f + 0.5f;
				directionalSource.Pixels[offset + 2] = direction.z * 0.5f + 0.5f;
				directionalSource.Pixels[offset + 3] = 1.0f;
			}
		}

		gl::CubemapFloatData directionalCube;
		bool conversionOrientationMatches =
			gl::EnvironmentMapLoader::ConvertEquirectangularToCubemap(
				directionalSource, 5, directionalCube);
		for (uint32_t face = 0; face < 6 && conversionOrientationMatches; ++face)
		{
			const size_t centerOffset = (2 * 5 + 2) * 4;
			const glm::vec3 sampledDirection{
				directionalCube.Faces[face][centerOffset + 0] * 2.0f - 1.0f,
				directionalCube.Faces[face][centerOffset + 1] * 2.0f - 1.0f,
				directionalCube.Faces[face][centerOffset + 2] * 2.0f - 1.0f
			};
			conversionOrientationMatches =
				glm::length(sampledDirection - expectedDirections[face]) < 0.08f;
		}
		context.Check(conversionOrientationMatches,
			"equirectangular sampling preserves all six cubemap orientations");

		const std::filesystem::path hdrPath = root / "environment.hdr";
		{
			std::ofstream stream(hdrPath, std::ios::binary);
			stream << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 4\n";
			const char rgbe[] = {
				static_cast<char>(128), static_cast<char>(64),
				static_cast<char>(32), static_cast<char>(130)
			};
			for (uint32_t pixel = 0; pixel < 8; ++pixel)
				stream.write(rgbe, sizeof(rgbe));
		}
		gl::FloatImageData decodedHDR;
		const bool decodedSuccessfully =
			gl::EnvironmentMapLoader::LoadEquirectangularHDR(hdrPath, decodedHDR);
		context.Check(decodedSuccessfully && decodedHDR.IsValid()
			&& decodedHDR.Width == 4 && decodedHDR.Height == 2
			&& decodedHDR.Pixels[0] > 1.0f,
			"Radiance HDR decoding retains linear high-range values");
		gl::AssetManager::Initialize(root);
		const gl::AssetHandle hdrHandle =
			gl::AssetManager::ImportAsset(hdrPath);
		context.Check(gl::AssetManager::GetMetadata(hdrHandle).Type
			== gl::AssetType::Cubemap,
			".hdr imports as a Cubemap asset");
		gl::AssetManager::Shutdown();
	}

}

int main(int argc, char** argv)
{
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	std::cout << "[RUN] Log initialization\n";
	gl::Log::Init();
	TestContext context;
	TemporaryDirectory temporaryDirectory;

	std::cout << "Glimmer headless regression tests\n";
	std::cout << "[RUN] Material round trip\n";
	TestMaterialRoundTrip(context, temporaryDirectory.Path());
	std::cout << "[RUN] Model importer foundation\n";
	TestModelImporterFoundation(context, temporaryDirectory.Path());
	std::cout << "[RUN] Cerberus FBX import\n";
	TestCerberusFBXImport(context);
	std::cout << "[RUN] Terrain material round trip\n";
	TestTerrainMaterialRoundTrip(context, temporaryDirectory.Path());
	std::cout << "[RUN] Terrain material registry\n";
	TestTerrainMaterialRegistry(context, temporaryDirectory.Path());
	std::cout << "[RUN] Material override merge\n";
	TestMaterialOverrideMerge(context, temporaryDirectory.Path());
	std::cout << "[RUN] Scene round trip\n";
	TestSceneRoundTrip(context, temporaryDirectory.Path());
	std::cout << "[RUN] Terrain copy and transactions\n";
	TestTerrainCopyAndTransactions(context);
	std::cout << "[RUN] Terrain presets\n";
	TestTerrainPresets(context);
	std::cout << "[RUN] Terrain chunk layout\n";
	TestTerrainChunkLayout(context);
	std::cout << "[RUN] Terrain hydrology runtime\n";
	TestTerrainHydrologyRuntime(context);
	std::cout << "[RUN] Terrain climate runtime\n";
	TestTerrainClimateRuntime(context);
	std::cout << "[RUN] Terrain camera frustum culling\n";
	TestTerrainCameraFrustumCulling(context);
	std::cout << "[RUN] Shadow frustum culling\n";
	TestShadowFrustumCulling(context);
	std::cout << "[RUN] Environment map foundation\n";
	TestEnvironmentMapFoundation(context, temporaryDirectory.Path());

	if (argc > 1 && std::string(argv[1]) == "--force-failure")
		context.Check(false, "intentional failure verifies non-zero exit propagation");

	if (context.FailureCount() == 0)
		std::cout << "[PASS] all headless regression tests passed\n";
	else
		std::cerr << "[FAIL] " << context.FailureCount() << " regression assertion(s) failed\n";

	return context.ExitCode();
}
