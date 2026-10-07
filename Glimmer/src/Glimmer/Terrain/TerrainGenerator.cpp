#include "glpch.h"
#include "TerrainGenerator.h"
#include "Glimmer/Terrain/TerrainSampling.h"
#include "Glimmer/Simulation/TerrainHydrologyGPU.h"

#include <cmath>
#include <chrono>
#include <fstream>
#include <sstream>

namespace gl {

	TerrainGenerator::TerrainGenerator(
		const SimulationGridSpecification& gridSpecification,
		const std::string& generationShaderPath,
		const std::string& erosionShaderPath,
		const std::string& derivationShaderPath, const std::string& stampShaderPath)
		: m_HeightGrid(gridSpecification),
		  m_GenerationShader(ComputeShader::Create(generationShaderPath, false)),
		  m_ErosionShader(ComputeShader::Create(erosionShaderPath, false)),
		  m_DerivationShader(ComputeShader::Create(derivationShaderPath, false)),
		  m_StampShaderPath(stampShaderPath.empty()
			? (std::filesystem::path(generationShaderPath).parent_path() / "ApplyTerrainStamp.comp").string()
			: stampShaderPath)
	{
		GL_CORE_ASSERT(gridSpecification.Format == TextureFormat::R32F,
			"TerrainGenerator currently requires an R32F height grid.");
		CreateDerivedTextures();
	}

	TerrainGenerator::TerrainGenerator(const SimulationGridSpecification& grid,
		const Ref<ComputeShader>& generation, const Ref<ComputeShader>& erosion,
		const Ref<ComputeShader>& derivation, const Ref<ComputeShader>& stamp)
		: m_HeightGrid(grid), m_GenerationShader(generation), m_ErosionShader(erosion),
		m_DerivationShader(derivation), m_StampShader(stamp)
	{
		CreateDerivedTextures();
	}

	bool TerrainGenerator::Generate(const TerrainSpecification& specification, float worldSize)
	{
		const auto validation = ValidateTerrainRecipe(specification.Recipe,
			specification.HeightScale, specification.DataVersion, specification.Procedural);
		if (!validation.Valid()) { m_LastGenerationError = validation.Message; return false; }
		const bool needsStamp = std::any_of(specification.Recipe.Stamps.begin(), specification.Recipe.Stamps.end(),
			[](const TerrainStamp& stamp) { return stamp.Enabled && stamp.Strength > 0.0f; });
		if (needsStamp && (m_HeightGrid.GetSpecification().Width < 2 || m_HeightGrid.GetSpecification().Height < 2
			|| !std::isfinite(worldSize) || worldSize <= 0.0f))
		{ m_LastGenerationError = "Stamp generation requires a finite positive range and endpoint grid."; return false; }
		if (needsStamp && !m_StampShader)
			m_StampShader = ComputeShader::Create(m_StampShaderPath, false);
		for (const auto& shader : { m_GenerationShader, m_ErosionShader, m_DerivationShader,
			needsStamp ? m_StampShader : m_GenerationShader })
		{
			if (!shader || shader->GetVersion() == 0 || !shader->GetLastReloadResult().Success)
			{
				m_LastGenerationError = shader ? shader->GetLastReloadResult().Message : "Compute Shader unavailable.";
				return false;
			}
		}
		try
		{
			TerrainGenerator candidate(m_HeightGrid.GetSpecification(), m_GenerationShader,
				m_ErosionShader, m_DerivationShader, m_StampShader);
			candidate.GenerateCandidate(specification, worldSize);
			std::swap(m_HeightGrid, candidate.m_HeightGrid);
			m_NormalSlopeMap.swap(candidate.m_NormalSlopeMap);
			m_AnalysisMap.swap(candidate.m_AnalysisMap);
			m_MaterialWeightMap.swap(candidate.m_MaterialWeightMap);
			m_RecipeClipMask.swap(candidate.m_RecipeClipMask);
			m_ProtectionMap.swap(candidate.m_ProtectionMap);
			m_LastDispatchCount = candidate.m_LastDispatchCount;
			m_DataVersion = candidate.m_DataVersion;
			m_HasGeneratedSurface = true;
			m_LastGenerationError.clear();
			return true;
		}
		catch (const std::exception& error) { m_LastGenerationError = error.what(); return false; }
	}

	void TerrainGenerator::GenerateCandidate(
		const TerrainSpecification& specification,
		float worldSize)
	{
		GL_PROFILE_FUNCTION();
		const TerrainNoiseSettings& settings = specification.Noise;
		m_LastDispatchCount = 0;
		m_DataVersion = std::clamp(specification.DataVersion, 1u, 2u);

		m_GenerationShader->Bind();
		m_GenerationShader->UploadUniformInt("u_TerrainDataVersion", m_DataVersion);
		m_GenerationShader->UploadUniformInt("u_SynthesisVersion",
			static_cast<int>(std::clamp(settings.SynthesisVersion, 1u, 2u)));
		m_GenerationShader->UploadUniformInt("u_Preset", static_cast<int>(specification.Preset));
		m_GenerationShader->UploadUniformInt("u_Seed", settings.Seed);
		m_GenerationShader->UploadUniformInt("u_Octaves", settings.Octaves);
		m_GenerationShader->UploadUniformFloat("u_Frequency", settings.Frequency);
		m_GenerationShader->UploadUniformFloat("u_WorldFrequencyScale",
			settings.WorldSpaceFrequency ? ClampTerrainWorldSize(worldSize) / 256.0f : 1.0f);
		m_GenerationShader->UploadUniformFloat("u_Lacunarity", settings.Lacunarity);
		m_GenerationShader->UploadUniformFloat("u_Persistence", settings.Persistence);
		m_GenerationShader->UploadUniformFloat("u_DomainWarp", settings.DomainWarp);
		m_GenerationShader->UploadUniformFloat("u_RidgeStrength", settings.RidgeStrength);
		m_GenerationShader->UploadUniformFloat("u_ContinentScale", settings.ContinentScale);
		m_GenerationShader->UploadUniformFloat("u_ErosionStrength", settings.ErosionStrength);
		m_GenerationShader->UploadUniformFloat("u_DetailStrength", settings.DetailStrength);
		m_GenerationShader->UploadUniformFloat("u_MountainDirection", settings.MountainDirection);
		m_GenerationShader->UploadUniformFloat("u_MountainWidth", settings.MountainWidth);
		m_GenerationShader->UploadUniformFloat("u_PlateauStrength", settings.PlateauStrength);
		m_GenerationShader->UploadUniformFloat("u_GeologyBlend",
			glm::clamp(settings.GeologyBlend, 0.0f, 1.0f));
		m_GenerationShader->UploadUniformFloat("u_GeologyScale",
			glm::clamp(settings.GeologyScale, 0.25f, 12.0f));
		m_GenerationShader->UploadUniformFloat("u_RiftStrength",
			glm::clamp(settings.RiftStrength, 0.0f, 0.5f));
		m_GenerationShader->UploadUniformFloat("u_TrendStrength",
			glm::clamp(settings.TrendStrength, 0.0f, 0.5f));
		m_GenerationShader->UploadUniformFloat2("u_Offset", settings.Offset);
		m_GenerationShader->BindImageTexture(
			0,
			m_HeightGrid.WriteTexture()->GetRendererID(),
			0,
			ImageAccess::Write,
			ImageFormat::R32F);

		Dispatch2D(m_GenerationShader);
		ComputeShader::Barrier();
		m_HeightGrid.Swap();

		RunThermalErosion(specification.Authoring, specification.HeightScale, worldSize);
		ApplyRecipe(specification, worldSize);
		DeriveMaps(specification.HeightScale, worldSize);
	}

	void TerrainGenerator::ApplyRecipe(const TerrainSpecification& specification, float worldSize)
	{
		for (const auto& stamp : specification.Recipe.Stamps)
		{
			if (!stamp.Enabled || stamp.Strength == 0.0f) continue;
			if (!m_RecipeClipMask)
			{
				TextureSpecification mask;
				mask.Width = m_HeightGrid.GetSpecification().Width;
				mask.Height = m_HeightGrid.GetSpecification().Height;
				mask.Format = TextureFormat::R32F;
				mask.MinFilter = mask.MagFilter = TextureFilter::Nearest;
				mask.WrapS = mask.WrapT = TextureWrap::ClampToEdge;
				mask.Usage = TextureUsage::Sampled | TextureUsage::Storage | TextureUsage::Readback;
				m_RecipeClipMask = Texture2D::Create(mask);
				m_RecipeClipMask->Clear(glm::vec4(0));
				m_ProtectionMap = Texture2D::Create(mask);
				m_ProtectionMap->Clear(glm::vec4(0));
			}
			const double angle = std::remainder(double(stamp.RotationDegrees), 360.0) * 0.017453292519943295;
			m_StampShader->Bind();
			m_StampShader->UploadUniformInt("u_Shape", static_cast<int>(stamp.Shape));
			m_StampShader->UploadUniformInt("u_Operation", static_cast<int>(stamp.Operation));
			m_StampShader->UploadUniformFloat2("u_Center", stamp.Center);
			m_StampShader->UploadUniformFloat2("u_Size", stamp.Size);
			m_StampShader->UploadUniformFloat2("u_Rotation", { float(std::cos(angle)), float(std::sin(angle)) });
			m_StampShader->UploadUniformFloat("u_TransitionWidth", stamp.TransitionWidth);
			m_StampShader->UploadUniformFloat("u_Strength", stamp.Strength);
			m_StampShader->UploadUniformFloat("u_Height", stamp.Height);
			m_StampShader->UploadUniformFloat("u_HeightScale", specification.HeightScale);
			m_StampShader->UploadUniformFloat("u_WorldSize", ClampTerrainWorldSize(worldSize));
			m_StampShader->BindImageTexture(0, m_HeightGrid.ReadTexture()->GetRendererID(), 0, ImageAccess::Read, ImageFormat::R32F);
			m_StampShader->BindImageTexture(1, m_HeightGrid.WriteTexture()->GetRendererID(), 0, ImageAccess::Write, ImageFormat::R32F);
			m_StampShader->BindImageTexture(2, m_RecipeClipMask->GetRendererID(), 0, ImageAccess::ReadWrite, ImageFormat::R32F);
			m_StampShader->BindImageTexture(3, m_ProtectionMap->GetRendererID(), 0, ImageAccess::ReadWrite, ImageFormat::R32F);
			Dispatch2D(m_StampShader);
			ComputeShader::Barrier();
			m_HeightGrid.Swap();
		}
	}

	uint64_t TerrainGenerator::ReadRecipeClippedNodeCount() const
	{
		if (!m_RecipeClipMask) return 0;
		std::vector<float> flags(size_t(m_RecipeClipMask->GetWidth()) * m_RecipeClipMask->GetHeight());
		m_RecipeClipMask->GetImageData(flags.data(), uint32_t(flags.size() * sizeof(float)));
		return std::count(flags.begin(), flags.end(), 1.0f);
	}

	void TerrainGenerator::Resize(uint32_t width, uint32_t height)
	{
		m_HeightGrid.Resize(width, height);
		CreateDerivedTextures();
		m_RecipeClipMask.reset();
		m_ProtectionMap.reset();
		m_HasGeneratedSurface = false;
	}

	void TerrainGenerator::DeriveMapsFromHeight(
		const Ref<Texture2D>& heightMap, float heightScale, float worldSize)
	{
		DeriveMaps(heightMap, heightScale, worldSize, false);
	}

	bool TerrainGenerator::ReloadShadersIfChanged()
	{
		bool changed = false;
		for (const Ref<ComputeShader>& shader : {
			m_GenerationShader, m_ErosionShader, m_DerivationShader, m_StampShader })
		{
			if (!shader) continue;
			const ShaderReloadResult result = shader->ReloadIfChanged();
			// Failed reloads also need publication validation and visible diagnostics.
			changed |= result.Attempted;
		}
		return changed;
	}

	void TerrainGenerator::CreateDerivedTextures()
	{
		const auto& grid = m_HeightGrid.GetSpecification();
		TextureSpecification specification;
		specification.Width = grid.Width;
		specification.Height = grid.Height;
		specification.Format = TextureFormat::RGBA16F;
		specification.MinFilter = TextureFilter::Linear;
		specification.MagFilter = TextureFilter::Linear;
		specification.WrapS = TextureWrap::ClampToEdge;
		specification.WrapT = TextureWrap::ClampToEdge;
		specification.Usage = TextureUsage::Sampled
			| TextureUsage::Storage | TextureUsage::Readback;
		m_NormalSlopeMap = Texture2D::Create(specification);
		m_AnalysisMap = Texture2D::Create(specification);
		m_MaterialWeightMap = Texture2D::Create(specification);
		m_NormalSlopeMap->Clear(glm::vec4(0.0f));
		m_AnalysisMap->Clear(glm::vec4(0.0f));
		m_MaterialWeightMap->Clear(glm::vec4(0.0f));
	}

	void TerrainGenerator::Dispatch2D(const Ref<ComputeShader>& shader,
		bool countGenerationDispatch)
	{
		const auto& specification = m_HeightGrid.GetSpecification();
		shader->Dispatch(
			(specification.Width + 7u) / 8u,
			(specification.Height + 7u) / 8u,
			1);
		if (countGenerationDispatch)
			++m_LastDispatchCount;
	}

	void TerrainGenerator::RunThermalErosion(
		const TerrainAuthoringSettings& settings, float heightScale, float worldSize)
	{
		if (!settings.EnableThermalErosion || settings.ThermalIterations == 0)
			return;

		m_ErosionShader->Bind();
		m_ErosionShader->UploadUniformInt("u_TerrainDataVersion", m_DataVersion);
		const auto& grid = m_HeightGrid.GetSpecification();
		m_ErosionShader->UploadUniformFloat2("u_TalusPerAxis", {
			TerrainTalusHeight(settings.StableSlopeDegrees,
				ClampTerrainWorldSize(worldSize) / std::max(grid.Width - 1u, 1u), heightScale),
			TerrainTalusHeight(settings.StableSlopeDegrees,
				ClampTerrainWorldSize(worldSize) / std::max(grid.Height - 1u, 1u), heightScale) });
		if (m_DataVersion >= 2 && (!std::isfinite(heightScale) || heightScale <= 0.0f))
			return;
		m_ErosionShader->UploadUniformFloat("u_Talus",
			glm::clamp(settings.Talus, 0.0001f, 0.25f));
		m_ErosionShader->UploadUniformFloat("u_Strength",
			std::isfinite(settings.ThermalStrength)
				? glm::clamp(settings.ThermalStrength, 0.0f, 0.5f) : 0.0f);
		const uint32_t iterations = std::min(settings.ThermalIterations, 128u);
		for (uint32_t iteration = 0; iteration < iterations; ++iteration)
		{
			m_ErosionShader->BindImageTexture(0,
				m_HeightGrid.ReadTexture()->GetRendererID(), 0,
				ImageAccess::Read, ImageFormat::R32F);
			m_ErosionShader->BindImageTexture(1,
				m_HeightGrid.WriteTexture()->GetRendererID(), 0,
				ImageAccess::Write, ImageFormat::R32F);
			Dispatch2D(m_ErosionShader);
			ComputeShader::Barrier();
			m_HeightGrid.Swap();
		}
	}

	void TerrainGenerator::DeriveMaps(float heightScale, float worldSize)
	{
		DeriveMaps(m_HeightGrid.ReadTexture(), heightScale, worldSize, true);
	}

	void TerrainGenerator::DeriveMaps(const Ref<Texture2D>& heightMap,
		float heightScale, float worldSize, bool countGenerationDispatch)
	{
		m_DerivationShader->Bind();
		m_DerivationShader->UploadUniformInt("u_TerrainDataVersion", m_DataVersion);
		m_DerivationShader->UploadUniformFloat("u_HeightScale",
			std::isfinite(heightScale) ? std::max(heightScale, 0.0f) : 0.0f);
		m_DerivationShader->UploadUniformFloat("u_WorldSize",
			ClampTerrainWorldSize(worldSize));
		m_DerivationShader->BindImageTexture(0,
			heightMap->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_DerivationShader->BindImageTexture(1,
			m_NormalSlopeMap->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::RGBA16F);
		m_DerivationShader->BindImageTexture(2,
			m_AnalysisMap->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::RGBA16F);
		m_DerivationShader->BindImageTexture(3,
			m_MaterialWeightMap->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::RGBA16F);
		Dispatch2D(m_DerivationShader, countGenerationDispatch);
		ComputeShader::Barrier();
	}

	TerrainValidationResult TerrainGenerator::ValidateSamplingContract(
		const std::string& generationPath, const std::string& erosionPath,
		const std::string& derivationPath)
	{
		TerrainValidationResult result;
		try
		{
			auto require = [](bool condition, const char* message) {
				if (!condition) throw std::runtime_error(message);
			};
			for (uint32_t resolution : { 512u, 1024u, 2048u })
			{
				SimulationGridSpecification grid;
				grid.Width = grid.Height = resolution;
				grid.Format = TextureFormat::R32F;
				TerrainGenerator generator(grid, generationPath, erosionPath, derivationPath);
				generator.m_DataVersion = 2;
				std::vector<float> height(size_t(resolution) * resolution);
				for (uint32_t z = 0; z < resolution; ++z)
					for (uint32_t x = 0; x < resolution; ++x)
						height[size_t(z) * resolution + x] = 0.5f
							+ 0.5f * (float(x) / (resolution - 1u) - 0.5f)
							+ 0.25f * (float(z) / (resolution - 1u) - 0.5f);
				generator.m_HeightGrid.ReadTexture()->SetData(height.data(), uint32_t(height.size() * sizeof(float)));
				generator.DeriveMaps(1024.0f, 1024.0f);
				std::vector<float> normals(height.size() * 4u);
				generator.m_NormalSlopeMap->GetImageData(normals.data(), uint32_t(normals.size() * sizeof(float)));
				const glm::vec3 expected = glm::normalize(glm::vec3(-0.5f, 1.0f, -0.25f));
				const float angle = std::atan(std::hypot(0.5f, 0.25f)) * 57.2957795f;
				for (uint32_t z : { 0u, resolution / 2u, resolution - 1u })
					for (uint32_t x : { 0u, resolution / 2u, resolution - 1u })
					{
						const size_t index = (size_t(z) * resolution + x) * 4u;
						const glm::vec3 normal = glm::vec3(normals[index], normals[index + 1], normals[index + 2]) * 2.0f - 1.0f;
						require(glm::length(normal - expected) < 0.002f
							&& std::abs(normals[index + 3] * 90.0f - angle) < 1.0f,
							"GPU plane slope or endpoint normal differs across resolutions.");
					}
				for (TerrainPreset preset : { TerrainPreset::Alpine, TerrainPreset::Plateau,
					TerrainPreset::RollingHills, TerrainPreset::Volcanic, TerrainPreset::ErodedValley })
				{
					TerrainSpecification specification;
					ApplyTerrainPreset(specification, preset);
					generator.Generate(specification, specification.WorldSize);
					const TerrainValidationResult baseline = generator.ValidateOutputs();
					require(baseline.Valid, "GPU preset baseline contains invalid terrain data.");
					GL_CORE_INFO("Terrain data v2 baseline: preset={}, resolution={}, worldSize={}, heightScale={}, hash={}, height=[{}, {}]",
						TerrainPresetToString(preset), resolution, specification.WorldSize,
						specification.HeightScale, baseline.Hash, baseline.HeightMinimum, baseline.HeightMaximum);
				}
			}
			SimulationGridSpecification grid;
			grid.Width = grid.Height = 5;
			grid.Format = TextureFormat::R32F;
			TerrainGenerator generator(grid, generationPath, erosionPath, derivationPath);
			generator.m_DataVersion = 2;
			std::vector<float> height(25), analysis(100), weights(100);
			float bowlSoil = 0.0f;
			for (int sign : { 1, -1 })
			{
				for (int z = 0; z < 5; ++z)
					for (int x = 0; x < 5; ++x)
						height[z * 5 + x] = 0.5f + sign * 0.03125f * float((x - 2) * (x - 2) + (z - 2) * (z - 2));
				generator.m_HeightGrid.ReadTexture()->SetData(height.data(), 100);
				generator.DeriveMaps(32.0f, 16.0f);
				generator.m_AnalysisMap->GetImageData(analysis.data(), 400);
				generator.m_MaterialWeightMap->GetImageData(weights.data(), 400);
				// Analytic Laplacian is +/-0.25; bounded encoding gives 0.6/0.4.
				require(std::abs(analysis[48] - (sign > 0 ? 0.6f : 0.4f)) < 0.001f,
					"GPU quadratic curvature has an incorrect sign or physical scale.");
				require(generator.ValidateOutputs().Valid, "GPU analytic maps are not finite/normalized.");
				if (sign > 0) bowlSoil = weights[49];
				else require(bowlSoil > weights[49], "Bowl soil weight does not exceed convex-hill soil weight.");
			}
			TerrainAuthoringSettings settings;
			settings.ThermalIterations = 4;
			for (int z = 0; z < 5; ++z)
				for (int x = 0; x < 5; ++x)
					height[z * 5 + x] = 0.5f + std::tan(34.0f * 0.01745329252f) * float(x - 2) * 4.0f / 32.0f;
			generator.m_HeightGrid.ReadTexture()->SetData(height.data(), 100);
			generator.RunThermalErosion(settings, 32.0f, 16.0f);
			std::vector<float> eroded(25);
			generator.GetHeightMap()->GetImageData(eroded.data(), 100);
			require(height == eroded, "A plane below the stable slope was thermally eroded.");
			height.assign(25, 0.2f);
			height[0] = 0.8f; // Corner source catches duplicated clamped boundary transfers.
			generator.m_HeightGrid.ReadTexture()->SetData(height.data(), 100);
			generator.RunThermalErosion(settings, 32.0f, 16.0f);
			generator.GetHeightMap()->GetImageData(eroded.data(), 100);
			double before = 0.0, after = 0.0;
			for (size_t index = 0; index < height.size(); ++index)
			{
				before += height[index]; after += eroded[index];
				require(std::isfinite(eroded[index]) && eroded[index] >= 0 && eroded[index] <= 1,
					"Thermal erosion produced invalid height.");
			}
			require(eroded[0] < height[0] && std::abs(after - before) < 1e-5,
				"Thermal corner transfer is inactive or not mass conserving.");
			generator.DeriveMaps(0.0f, 16.0f);
			require(generator.ValidateOutputs().Valid, "Zero height scale maps are not finite.");
			result.Valid = true;
			result.Message = "512/1024/2048 planar slope/endpoints, quadratic curvature/soil, stable slope and thermal corner mass PASS";
		}
		catch (const std::exception& error) { result.Message = error.what(); }
		return result;
	}

	TerrainValidationResult TerrainGenerator::ValidateRecipeContract(
		const std::string& generationPath, const std::string& erosionPath,
		const std::string& derivationPath)
	{
		TerrainValidationResult result;
		try
		{
			auto require = [](bool condition, const char* message) {
				if (!condition) throw std::runtime_error(message);
			};
			float maximumDifference = 0;
			float maximumProtectionDifference = 0;
			for (uint32_t resolution : { 65u, 129u })
			{
				SimulationGridSpecification grid;
				grid.Width = resolution; grid.Height = resolution + 16u;
				TerrainGenerator generator(grid, generationPath, erosionPath, derivationPath);
				TerrainSpecification spec;
				spec.Authoring.EnableThermalErosion = false;
				require(generator.Generate(spec, spec.WorldSize), "Empty recipe generation failed.");
				require(!generator.GetProtectionMap(), "Empty recipe allocates a protection map.");
				const auto baseHash = generator.ValidateOutputs().Hash;
				const auto baseDispatches = generator.GetLastDispatchCount();
				std::vector<float> base(size_t(grid.Width) * grid.Height), actual(base.size());
				auto readProtection = [&]() {
					std::vector<float> values(base.size(), 0.0f);
					if (generator.GetProtectionMap()) generator.GetProtectionMap()->GetImageData(values.data(), uint32_t(values.size() * sizeof(float)));
					return values;
				};
				std::vector<float> unionReference;
				generator.GetHeightMap()->GetImageData(base.data(), uint32_t(base.size() * sizeof(float)));
				TerrainStamp platform;
				platform.ID = 1; platform.Shape = TerrainStampShape::Rectangle;
				platform.Size = { 256, 160 }; platform.Height = spec.HeightScale * 0.4f;
				platform.TransitionWidth = 64; platform.RotationDegrees = 23;
				TerrainStamp crater = platform;
				crater.ID = 2; crater.Shape = TerrainStampShape::Ellipse;
				crater.Center = { 120, -70 }; crater.Size = { 200, 80 };
				crater.Operation = TerrainStampOperation::Add; crater.Height = -20;
				TerrainStamp clip = platform;
				clip.ID = 3; clip.Center = { spec.WorldSize * 0.5f, -spec.WorldSize * 0.5f };
				clip.Size = { 128, 128 }; clip.Height = std::numeric_limits<float>::max();
				clip.TransitionWidth = 0; clip.RotationDegrees = 0;
				TerrainStamp disabled = platform;
				disabled.ID = 4; disabled.Enabled = false;
				TerrainStamp zero = platform; zero.ID = 5; zero.Strength = 0;
				for (int scenario = 0; scenario < 8; ++scenario)
				{
					if (scenario == 0) spec.Recipe.Stamps = { platform };
					if (scenario == 1) spec.Recipe.Stamps = { platform, crater, clip, disabled, zero };
					if (scenario == 2) spec.Recipe.Stamps = { crater, platform };
					if (scenario == 3) spec.Recipe.Stamps = { disabled, zero };
					if (scenario == 5) spec.Recipe.Stamps = { clip, crater, platform, disabled, zero };
					if (scenario == 6) { auto noHeightChange = platform; noHeightChange.Operation = TerrainStampOperation::Add; noHeightChange.Height = 0; spec.Recipe.Stamps = { noHeightChange }; }
					if (scenario == 7) { auto fractional = platform; fractional.Strength = 0.375f; spec.Recipe.Stamps = { fractional }; }
					if (scenario == 4)
					{
						spec.Recipe.Stamps.clear();
						for (uint64_t i = 0; i < 64; ++i)
						{
							auto stamp = crater; stamp.ID = i + 100;
							stamp.Height = i % 2 ? 0.5f : -0.25f;
							stamp.Center += glm::vec2(float(i), -float(i));
							spec.Recipe.Stamps.push_back(stamp);
						}
					}
					require(generator.Generate(spec, spec.WorldSize), "Stamp candidate generation failed.");
					const auto generated = generator.ValidateOutputs();
					require(generated.Valid, "Stamp height/derived maps are invalid.");
					generator.GetHeightMap()->GetImageData(actual.data(), uint32_t(actual.size() * sizeof(float)));
					const auto protection = readProtection();
					if (scenario == 1) unionReference = protection;
					if (scenario == 5) require(protection == unionReference, "Protection max union depends on operation order.");
					if (scenario == 6) require(generated.Hash == baseHash && std::any_of(protection.begin(), protection.end(), [](float p) { return p > 0; }),
						"Zero height delta must protect its footprint without altering height/derived maps.");
					uint64_t expectedClipped = 0;
					TerrainSamplingGrid xGrid(grid.Width, spec.WorldSize, TerrainSampleLayout::EndpointNodes);
					TerrainSamplingGrid zGrid(grid.Height, spec.WorldSize, TerrainSampleLayout::EndpointNodes);
					for (uint32_t z = 0; z < grid.Height; ++z)
						for (uint32_t x = 0; x < grid.Width; ++x)
						{
							const size_t index = size_t(z) * grid.Width + x;
							const auto expected = EvaluateTerrainRecipe(spec.Recipe, { xGrid.Position(x), zGrid.Position(z) }, base[index], spec.HeightScale);
							require(expected.Validation.Valid(), "CPU stamp reference failed.");
							expectedClipped += expected.Clipped;
							maximumDifference = std::max(maximumDifference, std::abs(actual[index] - expected.NormalizedHeight));
							require(std::isfinite(protection[index]) && protection[index] >= 0 && protection[index] <= 1, "Protection node is outside [0, 1].");
							maximumProtectionDifference = std::max(maximumProtectionDifference, std::abs(protection[index] - expected.ProtectionWeight));
							const auto residual = EvaluateProtectedTerrainResidual((x % 2) ? 10.0f : -10.0f, expected.ProtectionWeight);
							const float gpuMaskResidual = ((x % 2) ? 10.0f : -10.0f) * (1.0f - protection[index]);
							require(residual.Validation.Valid() && std::abs(residual.Residual - gpuMaskResidual) <= 1e-4f,
								"GPU protection does not match analytic residual attenuation.");
						}
					require(maximumDifference <= 1e-5f, "CPU/GPU stamp height exceeds normalized tolerance 1e-5.");
					require(maximumProtectionDifference <= 1e-5f, "CPU/GPU protection exceeds tolerance 1e-5.");
					require(generator.ReadRecipeClippedNodeCount() == expectedClipped, "GPU clipping diagnostic differs from CPU.");
					if (scenario == 3)
						require(generated.Hash == baseHash && generator.GetLastDispatchCount() == baseDispatches
							&& !generator.GetRecipeClipMask() && !generator.GetProtectionMap(), "Disabled/zero-strength stamps alter the old path.");
					require(generator.Generate(spec, spec.WorldSize) && generator.ValidateOutputs().Hash == generated.Hash,
						"Stamp generation is not deterministic.");
					require(readProtection() == protection, "Protection generation is not deterministic.");
					const auto publishedHeight = generator.GetHeightMap();
					const auto publishedNormal = generator.GetNormalSlopeMap();
					const auto publishedMask = generator.GetRecipeClipMask();
					const auto publishedProtection = generator.GetProtectionMap();
					spec.Recipe.Version = 999;
					require(!generator.Generate(spec, spec.WorldSize) && generator.GetHeightMap() == publishedHeight
						&& generator.GetNormalSlopeMap() == publishedNormal && generator.GetRecipeClipMask() == publishedMask
						&& generator.GetProtectionMap() == publishedProtection && readProtection() == protection
						&& generator.ValidateOutputs().Hash == generated.Hash, "Rejected recipe changes the published surface.");
					spec.Recipe.Version = TerrainRecipe::CurrentVersion;
				}
				const auto publishedHash = generator.ValidateOutputs().Hash;
				const auto publishedProtectionMap = generator.GetProtectionMap();
				const auto publishedProtectionValues = readProtection();
				const auto originalStampShader = generator.m_StampShader;
				generator.m_StampShader = ComputeShader::Create((std::filesystem::temp_directory_path()
					/ "Glimmer-Missing-Stamp-Shader.comp").string(), false);
				require(!generator.Generate(spec, spec.WorldSize) && generator.ValidateOutputs().Hash == publishedHash,
					"Missing stamp shader modifies the published surface.");
				require(generator.GetProtectionMap() == publishedProtectionMap && readProtection() == publishedProtectionValues,
					"Missing stamp shader modifies protection.");
				generator.m_StampShader = originalStampShader;
				const auto invalidShaderPath = std::filesystem::temp_directory_path()
					/ ("Glimmer-Invalid-Stamp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".comp");
				{ std::ofstream invalidShader(invalidShaderPath); invalidShader << "#version 450 core\ninvalid shader syntax\n"; }
				generator.m_StampShader = ComputeShader::Create(invalidShaderPath.string(), false);
				std::filesystem::remove(invalidShaderPath);
				require(!generator.Generate(spec, spec.WorldSize) && generator.ValidateOutputs().Hash == publishedHash,
					"Invalid stamp shader modifies the published surface.");
				require(generator.GetProtectionMap() == publishedProtectionMap && readProtection() == publishedProtectionValues,
					"Invalid stamp shader modifies protection.");
				generator.m_StampShader = originalStampShader;
				require(generator.Generate(spec, spec.WorldSize), "Generation cannot recover after shader failure.");
				// Simulation consumes the composed static height and resets to that initial surface.
				const auto folder = std::filesystem::path(generationPath).parent_path();
				TerrainHydrologyGPU hydrology(grid.Width, grid.Height, folder / "HydrologyFlux.comp",
					folder / "HydrologyUpdate.comp", folder / "SedimentTransport.comp",
					folder / "SedimentCapacity.comp", folder / "ErosionDeposition.comp");
				hydrology.SetInitialHeightMap(generator.GetHeightMap(), spec.HeightScale, spec.WorldSize);
				hydrology.GetHeightTexture()->GetImageData(actual.data(), uint32_t(actual.size() * sizeof(float)));
				std::vector<float> composed(actual.size());
				generator.GetHeightMap()->GetImageData(composed.data(), uint32_t(composed.size() * sizeof(float)));
				require(actual == composed, "Simulation initial height does not use stamp output.");
				hydrology.Reset();
				hydrology.GetHeightTexture()->GetImageData(actual.data(), uint32_t(actual.size() * sizeof(float)));
				require(actual == composed, "Simulation Reset does not restore stamp output.");
			}
			result.Valid = true;
			std::ostringstream message;
			message << "65x81/129x145 CPU/GPU, 1/3/64 stamps, order, clipping, determinism, failure retention and simulation initial/Reset PASS; max normalized difference="
				<< std::scientific << maximumDifference << "; protection max union/zero delta/fractional strength/residual/retention PASS; max protection difference="
				<< maximumProtectionDifference;
			result.Message = message.str();
		}
		catch (const std::exception& error) { result.Message = error.what(); }
		return result;
	}

	TerrainValidationResult TerrainGenerator::ValidateOutputs() const
	{
		TerrainValidationResult result;
		const auto& specification = m_HeightGrid.GetSpecification();
		const size_t pixelCount = static_cast<size_t>(specification.Width)
			* static_cast<size_t>(specification.Height);
		std::vector<float> height(pixelCount);
		std::vector<float> normalSlope(pixelCount * 4u);
		std::vector<float> analysis(pixelCount * 4u);
		std::vector<float> weights(pixelCount * 4u);
		m_HeightGrid.ReadTexture()->GetImageData(height.data(),
			static_cast<uint32_t>(height.size() * sizeof(float)));
		m_NormalSlopeMap->GetImageData(normalSlope.data(),
			static_cast<uint32_t>(normalSlope.size() * sizeof(float)));
		m_AnalysisMap->GetImageData(analysis.data(),
			static_cast<uint32_t>(analysis.size() * sizeof(float)));
		m_MaterialWeightMap->GetImageData(weights.data(),
			static_cast<uint32_t>(weights.size() * sizeof(float)));

		auto finiteRange = [](const std::vector<float>& values,
			float minimum, float maximum) {
			return std::all_of(values.begin(), values.end(),
				[minimum, maximum](float value) {
					return std::isfinite(value)
						&& value >= minimum && value <= maximum;
				});
		};
		if (!finiteRange(height, 0.0f, 1.0f)
			|| !finiteRange(normalSlope, 0.0f, 1.0f)
			|| !finiteRange(analysis, 0.0f, 1.0f)
			|| !finiteRange(weights, 0.0f, 1.001f))
		{
			result.Message = "Terrain output contains NaN, Inf, or out-of-range values.";
			return result;
		}
		const auto [minimumHeight, maximumHeight] = std::minmax_element(
			height.begin(), height.end());
		result.HeightMinimum = *minimumHeight;
		result.HeightMaximum = *maximumHeight;
		double heightSum = 0.0;
		for (float value : height) heightSum += value;
		result.HeightMean = static_cast<float>(heightSum / height.size());
		double squaredDeviation = 0.0;
		for (float value : height)
		{
			const double deviation = value - result.HeightMean;
			squaredDeviation += deviation * deviation;
		}
		result.HeightStandardDeviation = static_cast<float>(std::sqrt(
			squaredDeviation / height.size()));

		for (size_t pixel = 0; pixel < pixelCount; ++pixel)
		{
			const size_t offset = pixel * 4u;
			const float weightSum = weights[offset] + weights[offset + 1]
				+ weights[offset + 2] + weights[offset + 3];
			if (std::abs(weightSum - 1.0f) > 0.01f)
			{
				result.Message = "Terrain material weights are not normalized.";
				return result;
			}
		}

		uint64_t hash = 1469598103934665603ull;
		auto hashValues = [&hash](const std::vector<float>& values) {
			const auto* bytes = reinterpret_cast<const uint8_t*>(values.data());
			const size_t byteCount = values.size() * sizeof(float);
			for (size_t index = 0; index < byteCount; ++index)
			{
				hash ^= bytes[index];
				hash *= 1099511628211ull;
			}
		};
		hashValues(height);
		hashValues(normalSlope);
		hashValues(analysis);
		hashValues(weights);
		result.Valid = true;
		result.Hash = hash;
		result.Message = "Height and derived maps are finite, bounded, and normalized.";
		return result;
	}

}


