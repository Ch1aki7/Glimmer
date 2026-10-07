#include "Glimmer/Core/Log.h"
#include "Glimmer/Core/LayerStack.h"
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
#include "Glimmer/Terrain/TerrainSampling.h"
#include "Glimmer/Terrain/TerrainMaterial.h"
#include "Glimmer/Simulation/TerrainHydrologyRuntime.h"
#include "Glimmer/Simulation/TerrainClimateRuntime.h"
#include "Editor/EditorCommand.h"
#include "Editor/EditorScenePreferences.h"
#include "Editor/TerrainRecipeEditor.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
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

	bool SameTerrainRecipe(const gl::TerrainRecipe& left, const gl::TerrainRecipe& right)
	{
		if (left.Version != right.Version || left.Stamps.size() != right.Stamps.size()) return false;
		for (size_t i = 0; i < left.Stamps.size(); ++i)
		{
			const auto& a = left.Stamps[i];
			const auto& b = right.Stamps[i];
			if (a.ID != b.ID || a.Enabled != b.Enabled || a.Shape != b.Shape
				|| a.Operation != b.Operation || !Near(a.Center, b.Center) || !Near(a.Size, b.Size)
				|| !Near(a.RotationDegrees, b.RotationDegrees) || !Near(a.TransitionWidth, b.TransitionWidth)
				|| !Near(a.Strength, b.Strength) || !Near(a.Height, b.Height)) return false;
		}
		return true;
	}

	bool SameTerrainSpecification(
		const gl::TerrainSpecification& left,
		const gl::TerrainSpecification& right)
	{
		const auto& leftNoise = left.Noise;
		const auto& rightNoise = right.Noise;
		return left.Procedural == right.Procedural
			&& left.ExecutionMode == right.ExecutionMode
			&& SameTerrainRecipe(left.Recipe, right.Recipe)
			&& left.DataVersion == right.DataVersion
			&& left.Preset == right.Preset
			&& left.HeightMapResolution == right.HeightMapResolution
			&& left.MeshResolution == right.MeshResolution
			&& Near(left.WorldSize, right.WorldSize)
			&& Near(left.HeightScale, right.HeightScale)
			&& left.HeightMapHandle == right.HeightMapHandle
			&& left.RenderShaderHandle == right.RenderShaderHandle
			&& left.GenerationShaderHandle == right.GenerationShaderHandle
			&& left.ErosionShaderHandle == right.ErosionShaderHandle
			&& left.DerivationShaderHandle == right.DerivationShaderHandle
			&& left.TerrainMaterialHandle == right.TerrainMaterialHandle
			&& leftNoise.SynthesisVersion == rightNoise.SynthesisVersion
			&& leftNoise.Seed == rightNoise.Seed
			&& leftNoise.WorldSpaceFrequency == rightNoise.WorldSpaceFrequency
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
			&& Near(left.Authoring.StableSlopeDegrees, right.Authoring.StableSlopeDegrees)
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
		gl::MaterialPass outlinePass;
		outlinePass.Name = "Outline";
		outlinePass.ShaderHandle = gl::AssetHandle(301);
		outlinePass.Order = 0;
		outlinePass.Cull = gl::CullMode::Front;
		outlinePass.Queue = gl::MaterialPassQueue::Opaque;
		outlinePass.FloatParameters["u_OutlineWidth"] = 0.025f;
		outlinePass.Float4Parameters["u_OutlineColor"] =
			{ 0.01f, 0.02f, 0.03f, 1.0f };
		gl::MaterialPass forwardPass;
		forwardPass.Name = "Forward";
		forwardPass.ShaderHandle = gl::AssetHandle(302);
		forwardPass.Order = 100;
		forwardPass.Cull = gl::CullMode::Back;
		forwardPass.FloatParameters["u_ToonLightThreshold"] = 0.72f;
		expected.Passes = { outlinePass, forwardPass };

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
		context.Check(actual.Passes == expected.Passes,
			"material pass order, render state, and generic parameters survive round trip");
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
		terrain.Specification.WorldSize = 4096.0f;
		terrain.Specification.HeightScale = 37.5f;
		terrain.Specification.HeightMapHandle = gl::AssetHandle(6001);
		terrain.Specification.RenderShaderHandle = gl::AssetHandle(6002);
		terrain.Specification.GenerationShaderHandle = gl::AssetHandle(6003);
		terrain.Specification.ErosionShaderHandle = gl::AssetHandle(6004);
		terrain.Specification.DerivationShaderHandle = gl::AssetHandle(6005);
		terrain.Specification.TerrainMaterialHandle = gl::AssetHandle(6006);
		terrain.Specification.Noise.Seed = 73;
		terrain.Specification.Noise.SynthesisVersion = 2;
		terrain.Specification.Noise.WorldSpaceFrequency = true;
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

		context.Check(gl::SceneSerializer(source).Serialize(path.string())
			&& std::filesystem::is_regular_file(path),
			"minimal scene is written and reports success");
		std::string savedSnapshot;
		std::string editedSnapshot;
		context.Check(gl::SceneSerializer(source).SerializeToString(savedSnapshot),
			"scene can produce an in-memory save snapshot");
		transform.Translation.x += 1.0f;
		context.Check(gl::SceneSerializer(source).SerializeToString(editedSnapshot)
			&& editedSnapshot != savedSnapshot,
			"scene save snapshot detects direct component edits");
		transform.Translation.x -= 1.0f;
		context.Check(gl::SceneSerializer(source).Serialize(path.string())
			&& !std::filesystem::exists(path.string() + ".tmp")
			&& !std::filesystem::exists(path.string() + ".bak"),
			"scene replacement is atomic and removes staging files");
		context.Check(!gl::SceneSerializer(source).Serialize(
			(directory / "missing" / "cannot-write.glimmer").string()),
			"scene serialization reports an unavailable output path");

		const std::filesystem::path interruptedBackup = path.string() + ".bak";
		std::filesystem::rename(path, interruptedBackup);
		gl::Ref<gl::Scene> restoredScene = gl::CreateRef<gl::Scene>();
		context.Check(gl::SceneSerializer(restoredScene).Deserialize(path.string())
			&& std::filesystem::is_regular_file(path)
			&& !std::filesystem::exists(interruptedBackup),
			"interrupted scene replacement recovers its last valid backup");
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
		std::string legacyTerrainSnapshot = savedSnapshot;
		for (const char* key : { "DataVersion:", "StableSlopeDegrees:", "ExecutionMode:" })
		{
			const size_t keyPosition = legacyTerrainSnapshot.find(key);
			context.Check(keyPosition != std::string::npos, "terrain snapshot records data contract fields");
			if (keyPosition != std::string::npos)
			{
				const size_t lineStart = legacyTerrainSnapshot.rfind('\n', keyPosition) + 1;
				const size_t lineEnd = legacyTerrainSnapshot.find('\n', keyPosition);
				legacyTerrainSnapshot.erase(lineStart, lineEnd - lineStart + 1);
			}
		}
		const size_t synthesisKey = legacyTerrainSnapshot.find(
			"SynthesisVersion:");
		context.Check(synthesisKey != std::string::npos,
			"new terrain snapshot records synthesis version");
		if (synthesisKey != std::string::npos)
		{
			const size_t lineStart = legacyTerrainSnapshot.rfind('\n', synthesisKey) + 1;
			const size_t lineEnd = legacyTerrainSnapshot.find('\n', synthesisKey);
			legacyTerrainSnapshot.erase(lineStart,
				lineEnd == std::string::npos ? std::string::npos
					: lineEnd - lineStart + 1);
		}
		const size_t frequencyKey = legacyTerrainSnapshot.find(
			"WorldSpaceFrequency:");
		context.Check(frequencyKey != std::string::npos,
			"new terrain snapshot records world-space frequency mode");
		if (frequencyKey != std::string::npos)
		{
			const size_t precedingNewline = legacyTerrainSnapshot.rfind('\n', frequencyKey);
			const size_t lineStart = precedingNewline == std::string::npos
				? 0 : precedingNewline + 1;
			const size_t lineEnd = legacyTerrainSnapshot.find('\n', frequencyKey);
			legacyTerrainSnapshot.erase(lineStart,
				lineEnd == std::string::npos ? std::string::npos
					: lineEnd - lineStart + 1);
		}
		const auto legacyTerrainPath = directory / "LegacyTerrain.glimmer";
		{
			std::ofstream legacyTerrainFile(legacyTerrainPath);
			legacyTerrainFile << legacyTerrainSnapshot;
		}
		gl::Ref<gl::Scene> legacyTerrainScene = gl::CreateRef<gl::Scene>();
		const bool legacyTerrainLoaded =
			gl::SceneSerializer(legacyTerrainScene).Deserialize(
				legacyTerrainPath.string());
		const gl::Entity legacyTerrainEntity = legacyTerrainLoaded
			? legacyTerrainScene->FindEntityByUUID(entityUUID) : gl::Entity{};
		context.Check(legacyTerrainLoaded,
			"legacy terrain snapshot without frequency mode remains readable");
		context.Check(static_cast<bool>(legacyTerrainEntity),
			"legacy terrain snapshot restores the original entity");
		context.Check(legacyTerrainLoaded && legacyTerrainEntity
			&& !legacyTerrainEntity.GetComponent<gl::TerrainComponent>()
				.Specification.Noise.WorldSpaceFrequency,
			"legacy terrain without frequency mode keeps normalized UV generation");
		context.Check(legacyTerrainLoaded && legacyTerrainEntity
			&& legacyTerrainEntity.GetComponent<gl::TerrainComponent>()
				.Specification.Noise.SynthesisVersion == 1,
			"legacy terrain without synthesis version keeps original generator");
		if (legacyTerrainLoaded && legacyTerrainEntity)
		{
			auto legacySpecification = legacyTerrainEntity
				.GetComponent<gl::TerrainComponent>().Specification;
			gl::ApplyTerrainPreset(legacySpecification, gl::TerrainPreset::Alpine);
			context.Check(legacySpecification.ExecutionMode == gl::TerrainExecutionMode::Simulation,
				"missing execution mode and preset changes preserve legacy simulation behavior");
			context.Check(legacySpecification.DataVersion == 1,
				"missing data version and preset changes retain legacy sampling and erosion");
			context.Check(legacySpecification.Noise.SynthesisVersion == 1,
				"preset changes preserve a legacy terrain synthesis version");
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

	void TestTerrainSurfaceSnapshot(TestContext& context)
	{
		using Status = gl::TerrainQueryStatus;
		gl::TerrainSurfaceSnapshot snapshot;
		const gl::TerrainSurfaceVersion version{ 41, 7 };
		context.Check(snapshot.Query({ 0, 0 }, version).Status == Status::NotReady, "uninitialized surface snapshot reports not ready");
		std::vector<float> heights;
		for (int z = 0; z < 3; ++z) for (int x = 0; x < 5; ++x)
			heights.push_back(float(0.5 + 0.02 * (-10 + 5 * x) + 0.01 * (-10 + 10 * z)));
		context.Check(snapshot.Initialize(5, 3, 20, 100, version, heights) == Status::Ready, "rectangular endpoint snapshot validates nodes and version");
		const auto plane = snapshot.Query({ 2.5f, -3 }, version);
		context.Check(plane.Status == Status::Ready && Near(plane.Height, 52)
			&& Near(plane.SlopeDegrees, float(std::atan(std::sqrt(5.0)) * 57.29577951308232))
			&& glm::length(plane.Normal - glm::normalize(glm::vec3(-2, 1, -1))) < 1e-5,
			"physical bilinear query matches analytic plane height, normal and slope");
		context.Check(Near(snapshot.Query({ -10, -10 }, version).Height, 20) && Near(snapshot.Query({ 10, 10 }, version).Height, 80),
			"both terrain endpoints remain queryable without extrapolation");
		context.Check(snapshot.Query({ 10.01f, 0 }, version).Status == Status::OutOfBounds
			&& snapshot.Query({ 0, -10.01f }, version).Status == Status::OutOfBounds, "surface query rejects bounds instead of clamping");
		context.Check(snapshot.Query({ NAN, 0 }, version).Status == Status::InvalidPosition
			&& snapshot.Query({ 0, INFINITY }, version).Status == Status::InvalidPosition, "surface query rejects non-finite coordinates");
		context.Check(snapshot.Query({ 0, 0 }, { 41, 8 }).Status == Status::StaleVersion
			&& snapshot.Query({ 0, 0 }, { 42, 7 }).Status == Status::StaleVersion && snapshot.Query({ 0, 0 }, {}).Status == Status::NotReady,
			"query checks generation, Runtime identity and unavailable publication");
		heights[0] = NAN;
		context.Check(snapshot.Initialize(5, 3, 20, 100, version, heights) == Status::InvalidData
			&& snapshot.Query({ 0, 0 }, version).Status == Status::Ready, "invalid replacement preserves complete CPU snapshot");
		context.Check(snapshot.Initialize(1, 2, 20, 100, version, { 0, 0 }) == Status::InvalidData
			&& snapshot.Initialize(2, 2, 0, 100, version, { 0, 0, 0, 0 }) == Status::InvalidData
			&& snapshot.Initialize(2, 2, 20, -1, version, { 0, 0, 0, 0 }) == Status::InvalidData
			&& snapshot.Initialize(2, 2, 20, 100, {}, { 0, 0, 0, 0 }) == Status::InvalidData
			&& snapshot.Initialize(2, 2, INFINITY, 100, version, { 0, 0, 0, 0 }) == Status::InvalidData
			&& snapshot.Initialize(2, 2, 20, INFINITY, version, { 0, 0, 0, 0 }) == Status::InvalidData
			&& snapshot.Initialize(2, 2, 20, 100, version, { 0, 0, 0 }) == Status::InvalidData
			&& snapshot.Initialize(2, 2, 20, 100, version, { 0, 0, 1.1f, 0 }) == Status::InvalidData,
			"snapshot rejects invalid dimensions, scales, version and storage range");
		snapshot.Initialize(2, 2, 2, 10, version, { 0, 0, 0, 1 });
		const auto saddle = snapshot.Query({ 0, 0 }, version);
		context.Check(Near(saddle.Height, 2.5f) && Near(saddle.SlopeDegrees, float(std::atan(std::sqrt(12.5)) * 57.29577951308232)),
			"nonplanar cell uses bilinear heightfield rather than mesh triangulation");
		snapshot.Initialize(2, 2, 2, 0, version, { 0, 1, 1, 0 });
		context.Check(snapshot.Query({ 0, 0 }, version).Height == 0 && snapshot.Query({ 0, 0 }, version).SlopeDegrees == 0,
			"zero height scale produces a flat physical query surface");
		gl::TerrainComponent unprepared;
		context.Check(gl::TerrainRenderer::CaptureSurfaceSnapshot(unprepared, snapshot) == Status::NotReady && !unprepared.Runtime,
			"explicit capture does not prepare or allocate unavailable Terrain Runtime");
	}

	void TestTerrainProtection(TestContext& context)
	{
		gl::TerrainRecipe recipe;
		auto sample = [&](glm::vec2 position) { return gl::EvaluateTerrainRecipe(recipe, position, 0.2f, 100); };
		context.Check(sample({ 0, 0 }).Validation.Valid() && sample({ 0, 0 }).ProtectionWeight == 0,
			"empty recipe has zero protection without changing the base-height contract");
		gl::TerrainStamp platform;
		platform.ID = 1; platform.Shape = gl::TerrainStampShape::Rectangle;
		platform.Size = { 10, 10 }; platform.TransitionWidth = 4;
		platform.Height = 60; platform.Strength = 0.4f;
		recipe.Stamps = { platform };
		context.Check(Near(sample({ 0, 0 }).ProtectionWeight, 0.4f)
			&& Near(sample({ 5, 0 }).ProtectionWeight, 0.4f)
			&& Near(sample({ 7, 0 }).ProtectionWeight, 0.2f) && sample({ 9, 0 }).ProtectionWeight == 0,
			"protection uses core, smooth outside transition and strength in local units");
		recipe.Stamps[0].Shape = gl::TerrainStampShape::Ellipse;
		context.Check(Near(sample({ 7, 0 }).ProtectionWeight, 0.2f) && sample({ 9, 0 }).ProtectionWeight == 0,
			"ellipse protection reuses the radial weight contract");
		recipe.Stamps[0] = platform;
		recipe.Stamps[0].Size = { 10, 4 }; recipe.Stamps[0].RotationDegrees = 90;
		recipe.Stamps[0].Center = { 8, -3 };
		context.Check(Near(sample({ 8, 1 }).ProtectionWeight, 0.4f) && Near(sample({ 12, -3 }).ProtectionWeight, 0.2f),
			"protection follows translated and rotated local stamp coordinates");
		auto add = platform; add.ID = 2; add.Strength = 0.8f;
		add.Operation = gl::TerrainStampOperation::Add; add.Height = 10;
		recipe.Stamps = { platform, add };
		const auto ordered = sample({ 0, 0 });
		std::swap(recipe.Stamps[0], recipe.Stamps[1]);
		const auto reversed = sample({ 0, 0 });
		context.Check(Near(ordered.ProtectionWeight, 0.8f) && ordered.ProtectionWeight == reversed.ProtectionWeight
			&& !Near(ordered.NormalizedHeight, reversed.NormalizedHeight),
			"protection combines with max rather than sum and is order-independent while height remains ordered");
		recipe.Stamps[0].Enabled = false; recipe.Stamps[1].Strength = 0;
		context.Check(sample({ 0, 0 }).ProtectionWeight == 0, "disabled and zero-strength operations contribute no protection");
		add.Strength = 1; add.Height = 0; recipe.Stamps = { add };
		context.Check(sample({ 0, 0 }).ProtectionWeight == 1 && sample({ 0, 0 }).NormalizedHeight == 0.2f
			&& !sample({ 0, 0 }).Clipped, "zero height delta protects a footprint independently of height changes and clipping");
		recipe.Stamps[0].Height = std::numeric_limits<float>::max();
		context.Check(sample({ 0, 0 }).ProtectionWeight == 1 && sample({ 0, 0 }).Clipped,
			"height clipping does not change protection union semantics");
		const auto core = gl::EvaluateProtectedTerrainResidual(12, 1);
		const auto transition = gl::EvaluateProtectedTerrainResidual(-12, 0.25f);
		const auto outside = gl::EvaluateProtectedTerrainResidual(12, 0);
		context.Check(core.Validation.Valid() && core.Residual == 0 && transition.Validation.Valid()
			&& transition.Residual == -9 && outside.Validation.Valid() && outside.Residual == 12,
			"analytic residual suppression is zero in protected cores, partial in transitions and unchanged outside");
		context.Check(gl::EvaluateProtectedTerrainResidual(std::numeric_limits<float>::max(), 0).Residual == std::numeric_limits<float>::max()
			&& gl::EvaluateProtectedTerrainResidual(-std::numeric_limits<float>::max(), 0.5f).Validation.Valid(),
			"finite extreme signed residuals remain finite under valid protection");
		context.Check(!gl::EvaluateProtectedTerrainResidual(INFINITY, 0).Validation.Valid()
			&& !gl::EvaluateProtectedTerrainResidual(1, NAN).Validation.Valid()
			&& !gl::EvaluateProtectedTerrainResidual(1, -0.1f).Validation.Valid()
			&& !gl::EvaluateProtectedTerrainResidual(1, 1.1f).Validation.Valid(),
			"residual attenuation explicitly rejects non-finite input and out-of-range protection");
		recipe.Version = 999;
		context.Check(!sample({ 0, 0 }).Validation.Valid() && sample({ 0, 0 }).ProtectionWeight == 0,
			"invalid recipe does not expose partially evaluated protection");
	}

	void TestTerrainRecipeEditor(TestContext& context)
	{
		gl::TerrainSpecification spec;
		context.Check(gl::TerrainRecipeEditor::Add(spec, gl::TerrainStampShape::Rectangle, gl::TerrainStampOperation::SetHeight)
			&& gl::TerrainRecipeEditor::Add(spec, gl::TerrainStampShape::Ellipse, gl::TerrainStampOperation::Add)
			&& spec.Recipe.Stamps[0].ID != 0 && spec.Recipe.Stamps[0].ID != spec.Recipe.Stamps[1].ID,
			"editor Add creates valid distinct stable IDs and neutral shape/operation defaults");
		const auto first = spec.Recipe.Stamps[0].ID, second = spec.Recipe.Stamps[1].ID;
		context.Check(gl::TerrainRecipeEditor::Move(spec.Recipe, 1, -1) && spec.Recipe.Stamps[0].ID == second
			&& spec.Recipe.Stamps[1].ID == first && !gl::TerrainRecipeEditor::Move(spec.Recipe, 0, -1)
			&& !gl::TerrainRecipeEditor::Move(spec.Recipe, 1, 1) && !gl::TerrainRecipeEditor::Move(spec.Recipe, 0, 0),
			"editor reorder preserves IDs and rejects boundary or invalid directions");
		context.Check(gl::TerrainRecipeEditor::Remove(spec.Recipe, 0) && spec.Recipe.Stamps[0].ID == first
			&& !gl::TerrainRecipeEditor::Remove(spec.Recipe, 9), "editor removal preserves remaining IDs and rejects invalid indices");
		while (spec.Recipe.Stamps.size() < gl::TerrainRecipe::MaximumStamps)
			gl::TerrainRecipeEditor::Add(spec, gl::TerrainStampShape::Ellipse, gl::TerrainStampOperation::Add);
		const auto full = spec.Recipe;
		context.Check(!gl::TerrainRecipeEditor::Add(spec, gl::TerrainStampShape::Ellipse, gl::TerrainStampOperation::Add)
			&& SameTerrainRecipe(full, spec.Recipe), "editor capacity rejection leaves the complete recipe untouched");
		spec.Recipe.Stamps.clear(); spec.DataVersion = 1;
		context.Check(!gl::TerrainRecipeEditor::Add(spec, gl::TerrainStampShape::Rectangle, gl::TerrainStampOperation::SetHeight)
			&& spec.Recipe.Stamps.empty() && spec.DataVersion == 1, "editor Add does not silently migrate a legacy terrain");
		spec.DataVersion = 2; spec.HeightScale = 0;
		context.Check(!gl::TerrainRecipeEditor::Add(spec, gl::TerrainStampShape::Rectangle, gl::TerrainStampOperation::SetHeight),
			"editor Add rejects a zero height scale before mutation");
		gl::TerrainComponent terrain;
		terrain.Runtime = gl::CreateRef<gl::TerrainRuntime>();
		terrain.Runtime->Dirty = false;
		const auto runtime = terrain.Runtime;
		gl::TerrainRecipeEditor::Add(terrain.Specification, gl::TerrainStampShape::Rectangle, gl::TerrainStampOperation::SetHeight);
		gl::EditorValueTransaction<gl::TerrainComponent> transaction;
		transaction.Begin(terrain);
		const float oldHeight = terrain.Specification.Recipe.Stamps[0].Height;
		for (float value : { 20.0f, 25.0f, 30.0f }) terrain.Specification.Recipe.Stamps[0].Height = value;
		gl::EditorCommandHistory history;
		history.PushExecuted(std::make_unique<gl::ValueEditorCommand<gl::TerrainComponent>>("Drag Stamp", transaction.GetBefore(), terrain,
			[&](const gl::TerrainComponent& value) { terrain.Specification = value.Specification; gl::TerrainRenderer::Invalidate(terrain); return true; }));
		transaction.Reset();
		context.Check(history.Undo() && !history.CanUndo() && terrain.Specification.Recipe.Stamps[0].Height == oldHeight
			&& terrain.Runtime == runtime && runtime->Dirty, "one continuous stamp drag produces one Undo and retains the fallback Runtime");
		context.Check(history.Redo() && terrain.Specification.Recipe.Stamps[0].Height == 30 && terrain.Runtime == runtime,
			"stamp drag Redo restores the final parameters without replacing Runtime");
	}

	void TestTerrainRecipe(TestContext& context, const std::filesystem::path& directory)
	{
		using Error = gl::TerrainRecipeError;
		gl::TerrainRecipe empty;
		context.Check(gl::ValidateTerrainRecipe(empty, 0.0f, 1, false).Valid()
			&& gl::EvaluateTerrainRecipe(empty, { 0, 0 }, 0.375f, 0.0f).NormalizedHeight == 0.375f,
			"empty recipe preserves legacy and zero-height terrain");
		gl::TerrainStamp platform;
		platform.ID = 9007199254740993ULL;
		platform.Shape = gl::TerrainStampShape::Rectangle;
		platform.Size = { 8, 8 };
		platform.TransitionWidth = 4;
		platform.Height = 60;
		gl::TerrainRecipe recipe;
		recipe.Stamps = { platform };
		auto evaluate = [&recipe](glm::vec2 position, float scale = 100.0f) {
			return gl::EvaluateTerrainRecipe(recipe, position, 20.0f / scale, scale);
		};
		context.Check(evaluate({ 0, 0 }).Validation.Valid()
			&& Near(evaluate({ 0, 0 }).NormalizedHeight, 0.6f)
			&& Near(evaluate({ 4, 0 }).NormalizedHeight, 0.6f),
			"SetHeight platform core and boundary use absolute local height");
		context.Check(Near(evaluate({ 6, 0 }).NormalizedHeight, 0.4f)
			&& Near(evaluate({ 8, 0 }).NormalizedHeight, 0.2f)
			&& Near(evaluate({ 100, 100 }).NormalizedHeight, 0.2f),
			"rectangle transition has analytic midpoint and strict outer support");
		context.Check(Near(evaluate({ 0, 0 }, 200).NormalizedHeight * 200, 60),
			"HeightScale changes storage rather than stamp target units");
		recipe.Stamps[0].Strength = 0.5f;
		context.Check(Near(evaluate({ 0, 0 }).NormalizedHeight, 0.4f),
			"stamp strength blends rather than changing target height");
		recipe.Stamps[0].Enabled = false;
		context.Check(Near(evaluate({ 0, 0 }).NormalizedHeight, 0.2f),
			"disabled stamp preserves the base surface");
		recipe.Stamps[0] = platform;
		recipe.Stamps[0].Size = { 8, 2 };
		recipe.Stamps[0].RotationDegrees = 90;
		recipe.Stamps[0].TransitionWidth = 0;
		context.Check(Near(evaluate({ 0, 3 }).NormalizedHeight, 0.6f)
			&& Near(evaluate({ 3, 0 }).NormalizedHeight, 0.2f),
			"rotated rectangle follows local Y rotation with a hard edge");
		recipe.Stamps[0].Center = { 7, -9 };
		context.Check(Near(evaluate({ 7, -6 }).NormalizedHeight, 0.6f)
			&& Near(evaluate({ 10, -9 }).NormalizedHeight, 0.2f),
			"stamp coordinates translate independently of terrain chunks");
		recipe.Stamps[0] = platform;
		recipe.Stamps[0].Shape = gl::TerrainStampShape::Ellipse;
		recipe.Stamps[0].Size = { 8, 4 };
		context.Check(Near(evaluate({ 4, 0 }).NormalizedHeight, 0.6f)
			&& Near(evaluate({ 6, 0 }).NormalizedHeight, 0.5375f)
			&& Near(evaluate({ 12, 0 }).NormalizedHeight, 0.2f),
			"ellipse transition uses the documented minor-radius radial metric");
		gl::TerrainStamp add = platform;
		add.ID = 42;
		add.Operation = gl::TerrainStampOperation::Add;
		add.Height = 10;
		recipe.Stamps = { platform, add };
		context.Check(Near(evaluate({ 0, 0 }).NormalizedHeight, 0.7f),
			"ordered SetHeight then Add accumulates local height");
		std::swap(recipe.Stamps[0], recipe.Stamps[1]);
		context.Check(Near(evaluate({ 0, 0 }).NormalizedHeight, 0.6f),
			"reordering overlapping stamps intentionally changes composition");
		recipe.Stamps = { add };
		recipe.Stamps[0].Height = -100;
		context.Check(evaluate({ 0, 0 }).Clipped && evaluate({ 0, 0 }).NormalizedHeight == 0,
			"negative Add clips to the terrain lower bound and reports clipping");
		recipe.Stamps[0].Height = 200;
		context.Check(evaluate({ 0, 0 }).Clipped && evaluate({ 0, 0 }).NormalizedHeight == 1
			&& !evaluate({ 100, 100 }).Clipped,
			"upper clipping respects stamp support rather than altering the whole terrain");
		recipe.Stamps[0].Height = std::numeric_limits<float>::max();
		context.Check(evaluate({ 0, 0 }).NormalizedHeight == 1
			&& evaluate({ 0, 0 }).Validation.Valid(),
			"finite extreme height parameters do not overflow the CPU reference");
		recipe.Stamps = { platform, add };
		auto expectInvalid = [&](gl::TerrainRecipe invalid, Error error, const char* message) {
			const auto validation = gl::ValidateTerrainRecipe(invalid, 100);
			context.Check(validation.Error == error && !validation.Message.empty(), message);
		};
		auto invalid = recipe;
		invalid.Version = 999;
		expectInvalid(invalid, Error::UnsupportedVersion, "unknown recipe version is rejected without migration");
		invalid = recipe; invalid.Stamps[1].ID = platform.ID;
		expectInvalid(invalid, Error::DuplicateID, "duplicate stable stamp IDs are rejected");
		context.Check(gl::ValidateTerrainRecipe(invalid, 100).StampIndex == 1,
			"recipe diagnostic identifies the failing stamp index");
		invalid = recipe; invalid.Stamps[0].ID = 0;
		expectInvalid(invalid, Error::InvalidID, "zero stamp ID is rejected");
		invalid = recipe; invalid.Stamps[0].Size.x = 0;
		expectInvalid(invalid, Error::InvalidSize, "zero stamp dimensions are rejected rather than repaired");
		invalid = recipe; invalid.Stamps[0].TransitionWidth = -1;
		expectInvalid(invalid, Error::InvalidTransition, "negative stamp transition width is rejected");
		invalid = recipe; invalid.Stamps[0].Strength = 1.1f;
		expectInvalid(invalid, Error::InvalidStrength, "stamp strength outside its contract is rejected");
		invalid = recipe; invalid.Stamps[0].Height = std::numeric_limits<float>::quiet_NaN();
		expectInvalid(invalid, Error::NonFiniteParameter, "NaN stamp height is rejected");
		invalid = recipe; invalid.Stamps[0].Center.x = std::numeric_limits<float>::infinity();
		expectInvalid(invalid, Error::NonFiniteParameter, "infinite stamp position is rejected");
		invalid = recipe; invalid.Stamps[0].Shape = static_cast<gl::TerrainStampShape>(77);
		expectInvalid(invalid, Error::InvalidShape, "unknown stamp shape is rejected");
		invalid = recipe; invalid.Stamps[0].Operation = static_cast<gl::TerrainStampOperation>(77);
		expectInvalid(invalid, Error::InvalidOperation, "unknown stamp operation is rejected");
		invalid = recipe; invalid.Stamps.resize(65);
		expectInvalid(invalid, Error::TooManyStamps, "stamp capacity is checked before evaluation");
		context.Check(gl::ValidateTerrainRecipe(recipe, 0).Error == Error::InvalidHeightScale
			&& gl::ValidateTerrainRecipe(recipe, -1).Error == Error::InvalidHeightScale
			&& gl::ValidateTerrainRecipe(recipe, std::numeric_limits<float>::infinity()).Error == Error::InvalidHeightScale,
			"nonempty recipes require a finite positive height scale");
		context.Check(gl::ValidateTerrainRecipe(recipe, 100, 1).Error == Error::UnsupportedTerrain
			&& gl::ValidateTerrainRecipe(recipe, 100, 2, false).Error == Error::UnsupportedTerrain,
			"recipe contract excludes legacy and imported terrain paths");
		context.Check(gl::EvaluateTerrainRecipe(recipe, { 0, 0 }, -0.1f, 100).Validation.Error == Error::InvalidSample
			&& gl::EvaluateTerrainRecipe(recipe, { INFINITY, 0 }, 0.2f, 100).Validation.Error == Error::InvalidSample,
			"CPU reference reports invalid source samples explicitly");

		const auto source = gl::CreateRef<gl::Scene>();
		auto entity = source->CreateEntity("Recipe Terrain");
		const auto uuid = entity.GetUUID();
		auto& spec = entity.AddComponent<gl::TerrainComponent>().Specification;
		spec.HeightScale = 100;
		context.Check(spec.ExecutionMode == gl::TerrainExecutionMode::Static, "new terrain defaults to resource-free static mode");
		spec.Recipe = recipe;
		spec.Recipe.Stamps[1].Enabled = false;
		spec.Recipe.Stamps[1].Center = { -3, 7 };
		spec.Recipe.Stamps[1].RotationDegrees = 23;
		spec.Recipe.Stamps[1].Strength = 0.35f;
		const auto path = directory / "TerrainRecipe.glimmer";
		context.Check(gl::SceneSerializer(source).Serialize(path.string()), "terrain recipe saves through Scene YAML");
		std::string yaml;
		gl::SceneSerializer(source).SerializeToString(yaml);
		const auto restored = gl::CreateRef<gl::Scene>();
		const bool loaded = gl::SceneSerializer(restored).Deserialize(path.string());
		const auto restoredEntity = restored->FindEntityByUUID(uuid);
		context.Check(loaded && restoredEntity && restoredEntity.GetComponent<gl::TerrainComponent>().Specification.ExecutionMode == gl::TerrainExecutionMode::Static,
			"explicit static mode survives scene round trip");
		spec.ExecutionMode = gl::TerrainExecutionMode(999);
		std::string invalidModeYaml;
		context.Check(!gl::SceneSerializer(source).SerializeToString(invalidModeYaml), "unknown execution mode is rejected before saving");
		spec.ExecutionMode = gl::TerrainExecutionMode::Static;
		context.Check(loaded && restoredEntity
			&& SameTerrainRecipe(spec.Recipe, restoredEntity.GetComponent<gl::TerrainComponent>().Specification.Recipe)
			&& !restoredEntity.GetComponent<gl::TerrainComponent>().Runtime,
			"recipe round trip preserves order, exact uint64 IDs and all stamp parameters without runtime");
		auto presetSpec = spec;
		gl::ApplyTerrainPreset(presetSpec, gl::TerrainPreset::Alpine);
		context.Check(SameTerrainRecipe(spec.Recipe, presetSpec.Recipe), "terrain preset switching preserves the authoring recipe");
		const auto runtimeScene = gl::Scene::Copy(source);
		auto& runtimeTerrain = runtimeScene->FindEntityByUUID(uuid).GetComponent<gl::TerrainComponent>();
		runtimeTerrain.Specification.Recipe.Stamps[0].Height = 31;
		context.Check(!runtimeTerrain.Runtime && Near(spec.Recipe.Stamps[0].Height, 60),
			"Play scene recipe edits do not contaminate the editor recipe");
		gl::TerrainComponent published;
		published.Runtime = gl::CreateRef<gl::TerrainRuntime>();
		published.Runtime->PublishedSpecification = published.Specification;
		published.Runtime->HasPublishedSpecification = true;
		published.Specification.HeightScale = 777;
		published.Specification.WorldSize = 2048;
		published.Specification.DataVersion = 1;
		const auto& surface = gl::TerrainRenderer::GetSurfaceSpecification(published);
		context.Check(surface.HeightScale != 777 && surface.WorldSize != 2048 && surface.DataVersion == 2,
			"render consumers retain published scale, extent and sampling version until regeneration succeeds");
		const gl::TerrainComponent copiedPublished = published;
		context.Check(!copiedPublished.Runtime && gl::TerrainRenderer::GetSurfaceSpecification(copiedPublished).HeightScale == 777,
			"copied terrain drops publication state and starts from its own desired specification");
		gl::TerrainComponent before = entity.GetComponent<gl::TerrainComponent>();
		gl::TerrainComponent after = before;
		std::swap(after.Specification.Recipe.Stamps[0], after.Specification.Recipe.Stamps[1]);
		gl::EditorCommandHistory history;
		history.Execute(std::make_unique<gl::ValueEditorCommand<gl::TerrainComponent>>(
			"Reorder Terrain Stamps", before, after, [source, uuid](const gl::TerrainComponent& value) {
				source->FindEntityByUUID(uuid).GetComponent<gl::TerrainComponent>() = value; return true;
			}));
		context.Check(SameTerrainRecipe(spec.Recipe, after.Specification.Recipe) && history.Undo()
			&& SameTerrainRecipe(spec.Recipe, before.Specification.Recipe) && history.Redo()
			&& SameTerrainRecipe(spec.Recipe, after.Specification.Recipe),
			"component command snapshots undo and redo the whole ordered recipe");
		history.Undo();
		auto rejectLoad = [&](std::string badYaml, const char* message) {
			const auto badPath = directory / "BadTerrainRecipe.glimmer";
			{ std::ofstream stream(badPath); stream << badYaml; }
			const auto destination = gl::CreateRef<gl::Scene>();
			destination->CreateEntity("Existing Entity");
			std::string original, unchanged;
			gl::SceneSerializer(destination).SerializeToString(original);
			const bool rejected = !gl::SceneSerializer(destination).Deserialize(badPath.string());
			gl::SceneSerializer(destination).SerializeToString(unchanged);
			context.Check(rejected && original == unchanged, message);
		};
		std::string futureYaml = yaml;
		std::string unknownModeYaml = yaml;
		const auto modePosition = unknownModeYaml.find("ExecutionMode: 0");
		unknownModeYaml.replace(modePosition, 16, "ExecutionMode: 999");
		rejectLoad(unknownModeYaml, "unknown execution mode fails before modifying the destination scene");
		const auto versionPosition = futureYaml.find("Version: 1", futureYaml.find("Recipe:"));
		futureYaml.replace(versionPosition, 10, "Version: 999");
		rejectLoad(futureYaml, "unknown recipe version fails before modifying the destination scene");
		std::string unknownShapeYaml = yaml;
		const auto shapePosition = unknownShapeYaml.find("Shape: Rectangle");
		unknownShapeYaml.replace(shapePosition, 16, "Shape: Unknown");
		rejectLoad(unknownShapeYaml, "unknown stamp enum fails rather than silently converting to a default");
		std::string malformedYaml = yaml;
		const auto sizePosition = malformedYaml.find("Size: [8, 8]");
		malformedYaml.replace(sizePosition, 12, "Size: [0, 8]");
		rejectLoad(malformedYaml, "invalid stamp size fails before scene entity creation");
		std::string savedOutput = "unchanged";
		spec.Recipe.Version = 999;
		context.Check(!gl::SceneSerializer(source).SerializeToString(savedOutput) && savedOutput == "unchanged"
			&& !gl::SceneSerializer(source).Serialize(path.string()),
			"unsupported in-memory recipe cannot be silently saved as another version");
		std::ifstream savedFile(path);
		const std::string retained((std::istreambuf_iterator<char>(savedFile)), std::istreambuf_iterator<char>());
		context.Check(retained == yaml, "rejected recipe save preserves the existing scene file");
		spec.Recipe = {};
		context.Check(gl::SceneSerializer(source).SerializeToString(savedOutput)
			&& savedOutput.find("Recipe:") == std::string::npos,
			"empty recipe does not change the legacy Scene YAML representation");
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
		after.Specification.DataVersion = 1;
		after.Specification.Authoring.StableSlopeDegrees = 48.0f;
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
			&& editorTerrain.Specification.DataVersion == 1
			&& Near(editorTerrain.Specification.Authoring.StableSlopeDegrees, 48.0f)
			&& !editorTerrain.Runtime,
			"terrain edit command applies specification and invalidates runtime");
		context.Check(history.Undo()
			&& Near(editorTerrain.Specification.HeightScale, 28.0f)
			&& editorTerrain.Specification.DataVersion == before.Specification.DataVersion
			&& Near(editorTerrain.Specification.Authoring.StableSlopeDegrees,
				before.Specification.Authoring.StableSlopeDegrees)
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
				&& first.HeightScale
					>= gl::TerrainWorldSizeDefault * 0.06f
				&& first.HeightScale
					<= gl::TerrainWorldSizeDefault * 0.17f
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
				std::string("terrain preset has world-scale relief and bounded authoring settings: ")
				+ gl::TerrainPresetToString(preset));
			previousSeed = first.Noise.Seed;
		}
		context.Check(gl::TerrainPresetFromString("unknown")
			== gl::TerrainPreset::Custom,
			"unknown terrain preset falls back to Custom");
		context.Check(Near(gl::ClampTerrainWorldSize(16384.0f),
				gl::TerrainWorldSizeMaximum)
			&& Near(gl::ClampTerrainWorldSize(
				std::numeric_limits<float>::quiet_NaN()),
				gl::TerrainWorldSizeDefault),
			"terrain world size applies finite runtime bounds");
	}

	void TestTerrainSamplingContract(TestContext& context)
	{
		using gl::TerrainSampleLayout;
		for (uint32_t count : { 512u, 1024u, 2048u })
		{
			const gl::TerrainSamplingGrid nodes(count, 1024.0f, TerrainSampleLayout::EndpointNodes);
			const gl::TerrainSamplingGrid cells(count, 1024.0f, TerrainSampleLayout::CellCenters);
			context.Check(Near(nodes.Position(0), -512.0f)
				&& Near(nodes.Position(count - 1), 512.0f), "node grid includes both endpoints");
			context.Check(Near(cells.Position(0), -512.0f + cells.Spacing() * 0.5f)
				&& Near(cells.Position(count - 1), 512.0f - cells.Spacing() * 0.5f),
				"cell grid partitions the domain without duplicated endpoints");
			for (uint32_t index : { 0u, count / 2u, count - 1u })
				context.Check(Near(nodes.TextureUV(nodes.Position(index)), (index + 0.5f) / count)
					&& Near(cells.TextureUV(cells.Position(index)), (index + 0.5f) / count),
					"position to texture UV lands at the same texel center");
			const double d = nodes.Spacing();
			const auto plane = gl::TerrainDerivePhysicalSample(0.5,
				0.5 - d * 0.5 / 1024.0, 0.5 + d * 0.5 / 1024.0,
				0.5 - d * 0.25 / 1024.0, 0.5 + d * 0.25 / 1024.0, d, 1024.0);
			const float expectedAngle = std::atan(std::hypot(0.5f, 0.25f)) * 57.2957795f;
			context.Check(Near(plane.SlopeDegrees, expectedAngle)
				&& std::abs(plane.Concavity) < 1e-8, "physical planar slope is resolution independent");
			const double quadratic = 0.00001 * d * d / 96.0;
			const auto bowl = gl::TerrainDerivePhysicalSample(0.5, 0.5 + quadratic,
				0.5 + quadratic, 0.5 + quadratic, 0.5 + quadratic, d, 96.0);
			const auto hill = gl::TerrainDerivePhysicalSample(0.5, 0.5 - quadratic,
				0.5 - quadratic, 0.5 - quadratic, 0.5 - quadratic, d, 96.0);
			context.Check(std::abs(bowl.Concavity - 0.00004) < 1e-8
				&& std::abs(hill.Concavity + 0.00004) < 1e-8,
				"quadratic curvature has a consistent world scale and bowl-positive sign");
			const float talus = gl::TerrainTalusHeight(35.0f, nodes.Spacing(), 96.0f);
			context.Check(std::abs(std::atan(talus * 96.0f / nodes.Spacing()) * 57.2957795f
				- 35.0f) < 0.001f, "talus converts to the same stable slope at every resolution");
			context.Check(Near(gl::TerrainTalusHeight(35.0f, nodes.Spacing() * std::sqrt(2.0f), 96.0f),
				talus * std::sqrt(2.0f)), "diagonal thermal thresholds use diagonal distances");
		}
		const auto flat = gl::TerrainDerivePhysicalSample(0.5, 0.5, 0.5, 0.5, 0.5, 1.0, 0.0);
		context.Check(flat.SlopeDegrees == 0.0f && flat.Concavity == 0.0,
			"zero vertical amplitude gives a flat finite surface");
		context.Check(gl::TerrainTalusHeight(35.0f, 1.0f, 0.0f) == 0.0f
			&& gl::TerrainTalusHeight(35.0f, 1.0f, std::numeric_limits<float>::infinity()) == 0.0f
			&& gl::TerrainStableSlope(std::numeric_limits<float>::quiet_NaN()) == 35.0f,
			"invalid thermal metrics have finite fallback semantics");
		bool invalidGridRejected = false;
		try { gl::TerrainSamplingGrid invalid(1, 1024.0f, TerrainSampleLayout::EndpointNodes); }
		catch (const std::invalid_argument&) { invalidGridRejected = true; }
		context.Check(invalidGridRejected, "node grids reject undefined one-sample spacing");
		bool invalidSampleRejected = false;
		try { gl::TerrainDerivePhysicalSample(std::numeric_limits<double>::quiet_NaN(), 0, 0, 0, 0, 1, 1); }
		catch (const std::invalid_argument&) { invalidSampleRejected = true; }
		context.Check(invalidSampleRejected, "CPU metric reference rejects nonfinite height inputs");
		context.Check(Near(gl::TerrainComposeHeight(20, 2, 19, 18), 21),
			"render composition adds simulation delta once, not absolute runtime height");
		const auto full = gl::TerrainEstimateTextureBudget(1024, 1024, 256, 2, 16);
		const auto reduced = gl::TerrainEstimateTextureBudget(1024, 512, 256, 2, 16);
		context.Check(full.GenerationBytes == 32ull * 1024 * 1024
			&& full.SimulationBytes == 116ull * 1024 * 1024
			&& full.DetailBytes == 16ull * 261 * 261 * 28
			&& full.TotalBytes() - reduced.TotalBytes() == 87ull * 1024 * 1024,
			"terrain budget includes ping-pong, halo and optional simulation decoupling");
		for (auto preset : { gl::TerrainPreset::Alpine, gl::TerrainPreset::Plateau,
			gl::TerrainPreset::RollingHills, gl::TerrainPreset::Volcanic, gl::TerrainPreset::ErodedValley })
		{
			gl::TerrainSpecification specification;
			specification.DataVersion = 1;
			gl::ApplyTerrainPreset(specification, preset);
			context.Check(specification.DataVersion == 1,
				"all presets preserve legacy data semantics independently of noise version");
		}
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

		const uint32_t largeAxis = gl::TerrainChunkLayout::SelectAxisCount(2048.0f);
		const auto largeChunks = gl::TerrainChunkLayout::Build(2048.0f, sharedResolution);
		bool largeCoverage = largeAxis == 8 && largeChunks.size() == 64;
		for (uint32_t z = 0; z < largeAxis; ++z)
			for (uint32_t x = 0; x < largeAxis; ++x)
			{
				const auto& chunk = largeChunks[z * largeAxis + x];
				largeCoverage &= Near(chunk.WorldSize, 256.0f)
					&& Near(chunk.UVOffset,
						glm::vec2(x, z) / static_cast<float>(largeAxis));
			}
		largeCoverage &= Near(largeChunks.front().LocalOffset
			- glm::vec2(largeChunks.front().WorldSize * 0.5f),
			glm::vec2(-1024.0f));
		largeCoverage &= Near(largeChunks.back().LocalOffset
			+ glm::vec2(largeChunks.back().WorldSize * 0.5f),
			glm::vec2(1024.0f));
		context.Check(largeCoverage,
			"large terrain uses continuous smaller tiles without stretching chunk bounds");
		std::vector<uint32_t> largeLevels(largeChunks.size(), 2u);
		largeLevels[0] = 0;
		const auto stableLarge = gl::TerrainChunkLayout::StabilizeNeighborLODs(
			std::move(largeLevels), largeAxis);
		context.Check(stableLarge.size() == largeChunks.size()
			&& stableLarge[1] == 1 && stableLarge[largeAxis] == 1,
			"large terrain constrains LOD across tile neighbors");
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

	void TestEditorScenePreferences(
		TestContext& context, const std::filesystem::path& directory)
	{
		const std::filesystem::path preferencesPath =
			directory / "EditorScenePreferences.txt";
#ifdef GL_PLATFORM_WINDOWS
		_putenv_s("GLIMMER_EDITOR_PREFERENCES_PATH",
			preferencesPath.string().c_str());
#else
		setenv("GLIMMER_EDITOR_PREFERENCES_PATH",
			preferencesPath.string().c_str(), 1);
#endif
		const std::filesystem::path projectRoot = directory / "ProjectA";
		const std::filesystem::path scenePath =
			projectRoot / "assets" / "Scenes" / "Last Scene.glimmer";
		const std::filesystem::path secondScenePath =
			projectRoot / "assets" / "Scenes" / "Second.glimmer";
		std::filesystem::create_directories(scenePath.parent_path());
		{
			std::ofstream scene(scenePath);
			scene << "Scene: Test\nEntities: []\n";
		}

		context.Check(gl::EditorScenePreferences::StoreLastScene(
			projectRoot, scenePath),
			"editor preferences persist the current scene path");
		const auto restored = gl::EditorScenePreferences::LoadLastScene(projectRoot);
		context.Check(restored && std::filesystem::equivalent(*restored, scenePath),
			"editor preferences restore a path containing spaces");
		context.Check(!gl::EditorScenePreferences::LoadLastScene(
			directory / "ProjectB"),
			"editor preferences do not leak scenes across projects");

		const gl::EditorCameraState firstCamera{
			{ 12.5f, -3.0f, 8.25f }, 42.0f, -31.0f, 127.0f };
		const gl::EditorCameraState secondCamera{
			{ -8.0f, 4.5f, 1.0f }, 7.5f, 18.0f, -62.0f };
		context.Check(gl::EditorScenePreferences::StoreCameraState(
			projectRoot, scenePath, firstCamera)
			&& gl::EditorScenePreferences::StoreCameraState(
				projectRoot, secondScenePath, secondCamera),
			"editor preferences persist per-scene camera states");
		const auto restoredFirstCamera =
			gl::EditorScenePreferences::LoadCameraState(projectRoot, scenePath);
		const auto restoredSecondCamera =
			gl::EditorScenePreferences::LoadCameraState(
				projectRoot, secondScenePath);
		context.Check(restoredFirstCamera
			&& Near(restoredFirstCamera->FocalPoint, firstCamera.FocalPoint)
			&& Near(restoredFirstCamera->Distance, firstCamera.Distance)
			&& Near(restoredFirstCamera->Pitch, firstCamera.Pitch)
			&& Near(restoredFirstCamera->Yaw, firstCamera.Yaw)
			&& restoredSecondCamera
			&& Near(restoredSecondCamera->FocalPoint, secondCamera.FocalPoint),
			"each scene restores its own editor camera state");
		gl::EditorCameraState invalidCamera = firstCamera;
		invalidCamera.Distance = std::numeric_limits<float>::quiet_NaN();
		context.Check(!gl::EditorScenePreferences::StoreCameraState(
			projectRoot, scenePath, invalidCamera),
			"non-finite editor camera state is rejected");
		context.Check(gl::EditorScenePreferences::StoreLastScene(
			projectRoot, std::nullopt)
			&& !gl::EditorScenePreferences::LoadLastScene(projectRoot),
			"new scene clears the persisted restore target");
		context.Check(gl::EditorScenePreferences::LoadCameraState(
			projectRoot, scenePath).has_value(),
			"new scene retains per-scene camera history");

		{
			std::ofstream legacy(preferencesPath,
				std::ios::binary | std::ios::trunc);
			legacy << "GLIMMER_EDITOR_SCENE_PREFERENCES 1\n"
				<< std::quoted(std::filesystem::weakly_canonical(
					projectRoot).generic_string()) << '\n'
				<< std::quoted(std::filesystem::weakly_canonical(
					scenePath).generic_string()) << '\n';
		}
		context.Check(gl::EditorScenePreferences::LoadLastScene(projectRoot)
			== std::filesystem::weakly_canonical(scenePath),
			"version 1 scene preferences remain readable");

#ifdef GL_PLATFORM_WINDOWS
		_putenv_s("GLIMMER_EDITOR_PREFERENCES_PATH", "");
#else
		unsetenv("GLIMMER_EDITOR_PREFERENCES_PATH");
#endif
	}

	void TestEditorCameraState(TestContext& context)
	{
		gl::EditorCamera camera;
		const gl::EditorCameraState requested{
			{ 3.0f, 5.0f, -7.0f }, 900.0f, -120.0f, 725.0f };
		context.Check(camera.SetState(requested),
			"finite editor camera state is accepted");
		const auto restored = camera.GetState();
		context.Check(Near(restored.FocalPoint, requested.FocalPoint)
			&& Near(restored.Distance, 900.0f)
			&& Near(restored.Pitch, -89.0f)
			&& Near(restored.Yaw, requested.Yaw),
			"editor camera state restoration applies runtime bounds");
		gl::EditorCameraState invalid = requested;
		invalid.Yaw = std::numeric_limits<float>::infinity();
		context.Check(!camera.SetState(invalid)
			&& Near(camera.GetState().Yaw, requested.Yaw),
			"invalid editor camera state leaves the current view unchanged");
	}

	class TeardownProbeLayer : public gl::Layer
	{
	public:
		TeardownProbeLayer(int identifier, std::vector<int>& detachOrder)
			: m_Identifier(identifier), m_DetachOrder(detachOrder) {}
		void OnDetach() override { m_DetachOrder.push_back(m_Identifier); }
	private:
		int m_Identifier;
		std::vector<int>& m_DetachOrder;
	};

	void TestLayerTeardown(TestContext& context)
	{
		std::vector<int> detachOrder;
		{
			gl::LayerStack stack;
			stack.PushLayer(new TeardownProbeLayer(1, detachOrder));
			stack.PushOverlay(new TeardownProbeLayer(2, detachOrder));
			stack.DetachAll();
			stack.DetachAll();
		}
		context.Check(detachOrder == std::vector<int>({ 2, 1 }),
			"layer teardown is reverse-order and idempotent");
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
	std::cout << "[RUN] Editor scene preferences\n";
	TestEditorScenePreferences(context, temporaryDirectory.Path());
	std::cout << "[RUN] Editor camera state\n";
	TestEditorCameraState(context);
	std::cout << "[RUN] Layer teardown\n";
	TestLayerTeardown(context);
	std::cout << "[RUN] Terrain copy and transactions\n";
	TestTerrainCopyAndTransactions(context);
	std::cout << "[RUN] Terrain recipe contract and persistence\n";
	TestTerrainRecipe(context, temporaryDirectory.Path());
	TestTerrainRecipeEditor(context);
	TestTerrainProtection(context);
	TestTerrainSurfaceSnapshot(context);
	std::cout << "[RUN] Terrain presets\n";
	TestTerrainPresets(context);
	std::cout << "[RUN] Terrain sampling contract\n";
	TestTerrainSamplingContract(context);
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
