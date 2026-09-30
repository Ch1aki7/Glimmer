#include "glpch.h"
#include "TerrainGenerator.h"
#include "Glimmer/Terrain/TerrainSampling.h"

#include <cmath>

namespace gl {

	TerrainGenerator::TerrainGenerator(
		const SimulationGridSpecification& gridSpecification,
		const std::string& generationShaderPath,
		const std::string& erosionShaderPath,
		const std::string& derivationShaderPath)
		: m_HeightGrid(gridSpecification),
		  m_GenerationShader(ComputeShader::Create(generationShaderPath)),
		  m_ErosionShader(ComputeShader::Create(erosionShaderPath)),
		  m_DerivationShader(ComputeShader::Create(derivationShaderPath))
	{
		GL_CORE_ASSERT(gridSpecification.Format == TextureFormat::R32F,
			"TerrainGenerator currently requires an R32F height grid.");
		CreateDerivedTextures();
	}

	void TerrainGenerator::Generate(
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
		DeriveMaps(specification.HeightScale, worldSize);
	}

	void TerrainGenerator::Resize(uint32_t width, uint32_t height)
	{
		m_HeightGrid.Resize(width, height);
		CreateDerivedTextures();
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
			m_GenerationShader, m_ErosionShader, m_DerivationShader })
		{
			const ShaderReloadResult result = shader->ReloadIfChanged();
			changed |= result.Attempted && result.Success;
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


