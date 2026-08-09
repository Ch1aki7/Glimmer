#include "glpch.h"
#include "TerrainGenerator.h"

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

		m_GenerationShader->Bind();
		m_GenerationShader->UploadUniformInt("u_Preset", static_cast<int>(specification.Preset));
		m_GenerationShader->UploadUniformInt("u_Seed", settings.Seed);
		m_GenerationShader->UploadUniformInt("u_Octaves", settings.Octaves);
		m_GenerationShader->UploadUniformFloat("u_Frequency", settings.Frequency);
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

		RunThermalErosion(specification.Authoring);
		DeriveMaps(specification.HeightScale, worldSize);
	}

	void TerrainGenerator::Resize(uint32_t width, uint32_t height)
	{
		m_HeightGrid.Resize(width, height);
		CreateDerivedTextures();
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

	void TerrainGenerator::Dispatch2D(const Ref<ComputeShader>& shader)
	{
		const auto& specification = m_HeightGrid.GetSpecification();
		shader->Dispatch(
			(specification.Width + 7u) / 8u,
			(specification.Height + 7u) / 8u,
			1);
		++m_LastDispatchCount;
	}

	void TerrainGenerator::RunThermalErosion(
		const TerrainAuthoringSettings& settings)
	{
		if (!settings.EnableThermalErosion || settings.ThermalIterations == 0)
			return;

		m_ErosionShader->Bind();
		m_ErosionShader->UploadUniformFloat("u_Talus",
			glm::clamp(settings.Talus, 0.0001f, 0.25f));
		m_ErosionShader->UploadUniformFloat("u_Strength",
			glm::clamp(settings.ThermalStrength, 0.0f, 0.5f));
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
		m_DerivationShader->Bind();
		m_DerivationShader->UploadUniformFloat("u_HeightScale",
			std::max(heightScale, 0.0f));
		m_DerivationShader->UploadUniformFloat("u_WorldSize",
			std::max(worldSize, 0.0001f));
		m_DerivationShader->BindImageTexture(0,
			m_HeightGrid.ReadTexture()->GetRendererID(), 0,
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
		Dispatch2D(m_DerivationShader);
		ComputeShader::Barrier();
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


