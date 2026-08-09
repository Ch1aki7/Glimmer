#include "Glimmer/Core/Log.h"
#include "Glimmer/Renderer/Material.h"
#include "Glimmer/Renderer/MaterialInstance.h"
#include "Glimmer/Scene/Components.h"
#include "Glimmer/Scene/Entity.h"
#include "Glimmer/Scene/Scene.h"
#include "Glimmer/Scene/SceneSerializer.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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

	bool Near(const glm::vec4& left, const glm::vec4& right)
	{
		return Near(left.x, right.x) && Near(left.y, right.y)
			&& Near(left.z, right.z) && Near(left.w, right.w);
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
	}

}

int main(int argc, char** argv)
{
	gl::Log::Init();
	TestContext context;
	TemporaryDirectory temporaryDirectory;

	std::cout << "Glimmer headless regression tests\n";
	TestMaterialRoundTrip(context, temporaryDirectory.Path());
	TestMaterialOverrideMerge(context, temporaryDirectory.Path());
	TestSceneRoundTrip(context, temporaryDirectory.Path());

	if (argc > 1 && std::string(argv[1]) == "--force-failure")
		context.Check(false, "intentional failure verifies non-zero exit propagation");

	if (context.FailureCount() == 0)
		std::cout << "[PASS] all headless regression tests passed\n";
	else
		std::cerr << "[FAIL] " << context.FailureCount() << " regression assertion(s) failed\n";

	return context.ExitCode();
}
