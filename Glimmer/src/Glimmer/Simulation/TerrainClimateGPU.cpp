#include "glpch.h"
#include "Glimmer/Simulation/TerrainClimateGPU.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace gl {

	namespace {
		SimulationGridSpecification MakeClimateGrid(
			uint32_t width, uint32_t height)
		{
			SimulationGridSpecification specification;
			specification.Width = width;
			specification.Height = height;
			specification.Format = TextureFormat::R32F;
			specification.Filter = TextureFilter::Nearest;
			specification.Wrap = TextureWrap::ClampToEdge;
			return specification;
		}

		Ref<Texture2D> MakeClimateTexture(uint32_t width, uint32_t height)
		{
			TextureSpecification specification;
			specification.Width = width;
			specification.Height = height;
			specification.Format = TextureFormat::R32F;
			specification.MinFilter = TextureFilter::Nearest;
			specification.MagFilter = TextureFilter::Nearest;
			specification.WrapS = TextureWrap::ClampToEdge;
			specification.WrapT = TextureWrap::ClampToEdge;
			specification.Usage = TextureUsage::Sampled
				| TextureUsage::Storage | TextureUsage::Readback;
			return Texture2D::Create(specification);
		}

		bool IsValidField(const std::vector<float>& values, size_t size)
		{
			return values.size() == size
				&& std::all_of(values.begin(), values.end(),
					[](float value) {
						return std::isfinite(value) && value >= 0.0f;
					});
		}

		Ref<Texture2D> MakeClimateInputTexture(
			uint32_t width, uint32_t height,
			const std::vector<float>& values)
		{
			Ref<Texture2D> texture = MakeClimateTexture(width, height);
			texture->SetData(values.data(),
				static_cast<uint32_t>(values.size() * sizeof(float)));
			return texture;
		}
	}

	TerrainClimateGPU::TerrainClimateGPU(
		uint32_t width, uint32_t height,
		std::filesystem::path sourceShaderPath,
		std::filesystem::path advectionShaderPath,
		std::filesystem::path responseShaderPath)
		: m_Temperature(MakeClimateGrid(width, height)),
		  m_AtmosphericMoisture(MakeClimateGrid(width, height)),
		  m_VegetationPotential(MakeClimateGrid(width, height)),
		  m_Rainfall(MakeClimateTexture(width, height)),
		  m_ZeroSurfaceWater(MakeClimateTexture(width, height)),
		  m_SourceShader(ComputeShader::Create(sourceShaderPath.string())),
		  m_AdvectionShader(ComputeShader::Create(advectionShaderPath.string())),
		  m_ResponseShader(ComputeShader::Create(responseShaderPath.string()))
	{
		Reset();
	}

	uint32_t TerrainClimateGPU::Advance(float frameDeltaSeconds,
		const Ref<Texture2D>& heightMap,
		const Ref<Texture2D>& surfaceWater,
		float heightScale, float worldSize)
	{
		if (!std::isfinite(frameDeltaSeconds) || frameDeltaSeconds <= 0.0f)
			return 0;
		m_Accumulator += frameDeltaSeconds;
		const double fixedTimeStep = std::max(
			static_cast<double>(m_Settings.FixedTimeStep), 1.0e-6);
		uint32_t steps = 0;
		while (m_Accumulator + std::numeric_limits<double>::epsilon()
			>= fixedTimeStep && steps < std::max(m_Settings.MaxSubsteps, 1u))
		{
			Step(heightMap, surfaceWater, heightScale, worldSize);
			m_Accumulator -= fixedTimeStep;
			++steps;
		}
		if (steps == std::max(m_Settings.MaxSubsteps, 1u)
			&& m_Accumulator >= fixedTimeStep)
		{
			const double retained = std::fmod(m_Accumulator, fixedTimeStep);
			m_Statistics.DroppedTime += m_Accumulator - retained;
			m_Accumulator = retained;
		}
		m_Statistics.Accumulator = m_Accumulator;
		return steps;
	}

	void TerrainClimateGPU::SingleStep(
		const Ref<Texture2D>& heightMap,
		const Ref<Texture2D>& surfaceWater,
		float heightScale, float worldSize)
	{
		Step(heightMap, surfaceWater, heightScale, worldSize);
	}

	void TerrainClimateGPU::Reset()
	{
		const float initialTemperature = std::isfinite(
			m_Settings.SeaLevelTemperature)
			? m_Settings.SeaLevelTemperature : 20.0f;
		const float initialMoisture = std::isfinite(
			m_Settings.InitialAtmosphericMoisture)
			? std::max(m_Settings.InitialAtmosphericMoisture, 0.0f) : 0.0f;
		m_Temperature.Clear(glm::vec4(initialTemperature, 0.0f, 0.0f, 0.0f));
		m_AtmosphericMoisture.Clear(
			glm::vec4(initialMoisture, 0.0f, 0.0f, 0.0f));
		m_VegetationPotential.Clear();
		m_Rainfall->Clear(glm::vec4(0.0f));
		m_ZeroSurfaceWater->Clear(glm::vec4(0.0f));
		m_Accumulator = 0.0;
		m_Statistics = {};
	}

	void TerrainClimateGPU::SetAtmosphericMoisture(
		const std::vector<float>& moistureDepth)
	{
		const auto& specification = m_AtmosphericMoisture.GetSpecification();
		const size_t cellCount = static_cast<size_t>(specification.Width)
			* specification.Height;
		if (!IsValidField(moistureDepth, cellCount))
			throw std::invalid_argument(
				"GPU climate atmospheric moisture field is invalid.");
		const uint32_t dataSize = static_cast<uint32_t>(
			moistureDepth.size() * sizeof(float));
		m_AtmosphericMoisture.ReadTexture()->SetData(
			moistureDepth.data(), dataSize);
		m_AtmosphericMoisture.WriteTexture()->SetData(
			moistureDepth.data(), dataSize);
		m_Statistics.ReadbackAvailable = false;
	}

	void TerrainClimateGPU::ReadbackStatistics(
		const Ref<Texture2D>& surfaceWater, float worldSize)
	{
		const auto& specification = m_Temperature.GetSpecification();
		const size_t cellCount = static_cast<size_t>(specification.Width)
			* specification.Height;
		std::vector<float> temperature(cellCount);
		std::vector<float> moisture(cellCount);
		std::vector<float> rainfall(cellCount);
		std::vector<float> vegetation(cellCount);
		std::vector<float> water(cellCount, 0.0f);
		m_Temperature.ReadTexture()->GetImageData(
			temperature.data(),
			static_cast<uint32_t>(temperature.size() * sizeof(float)));
		m_AtmosphericMoisture.ReadTexture()->GetImageData(
			moisture.data(),
			static_cast<uint32_t>(moisture.size() * sizeof(float)));
		m_Rainfall->GetImageData(rainfall.data(),
			static_cast<uint32_t>(rainfall.size() * sizeof(float)));
		m_VegetationPotential.ReadTexture()->GetImageData(
			vegetation.data(),
			static_cast<uint32_t>(vegetation.size() * sizeof(float)));
		const Ref<Texture2D>& resolvedWater = ResolveSurfaceWater(surfaceWater);
		resolvedWater->GetImageData(water.data(),
			static_cast<uint32_t>(water.size() * sizeof(float)));

		const double cellSize = static_cast<double>(
			std::max(worldSize, 0.0001f))
			/ std::max(specification.Width - 1u, 1u);
		const double cellArea = cellSize * cellSize;
		m_Statistics.AtmosphericWaterVolume =
			std::accumulate(moisture.begin(), moisture.end(), 0.0) * cellArea;
		m_Statistics.SurfaceWaterVolume =
			std::accumulate(water.begin(), water.end(), 0.0) * cellArea;
		m_Statistics.RainfallVolume =
			std::accumulate(rainfall.begin(), rainfall.end(), 0.0) * cellArea;
		m_Statistics.MinimumTemperature =
			*std::min_element(temperature.begin(), temperature.end());
		m_Statistics.MaximumTemperature =
			*std::max_element(temperature.begin(), temperature.end());
		m_Statistics.MinimumAtmosphericMoisture =
			*std::min_element(moisture.begin(), moisture.end());
		m_Statistics.MaximumAtmosphericMoisture =
			*std::max_element(moisture.begin(), moisture.end());
		m_Statistics.MaximumRainfall =
			*std::max_element(rainfall.begin(), rainfall.end());
		m_Statistics.MinimumVegetationPotential =
			*std::min_element(vegetation.begin(), vegetation.end());
		m_Statistics.MaximumVegetationPotential =
			*std::max_element(vegetation.begin(), vegetation.end());
		m_Statistics.Finite =
			std::all_of(temperature.begin(), temperature.end(),
				[](float value) { return std::isfinite(value); })
			&& std::all_of(moisture.begin(), moisture.end(),
				[](float value) {
					return std::isfinite(value) && value >= 0.0f;
				})
			&& std::all_of(rainfall.begin(), rainfall.end(),
				[](float value) {
					return std::isfinite(value) && value >= 0.0f;
				})
			&& std::all_of(vegetation.begin(), vegetation.end(),
				[](float value) {
					return std::isfinite(value)
						&& value >= 0.0f && value <= 1.0f;
				});
		m_Statistics.ReadbackAvailable = true;
	}

	bool TerrainClimateGPU::ReloadShadersIfChanged()
	{
		const ShaderReloadResult source = m_SourceShader->ReloadIfChanged();
		const ShaderReloadResult advection =
			m_AdvectionShader->ReloadIfChanged();
		const ShaderReloadResult response =
			m_ResponseShader->ReloadIfChanged();
		return (source.Attempted && source.Success)
			|| (advection.Attempted && advection.Success)
			|| (response.Attempted && response.Success);
	}

	TerrainClimateGPUValidationResult TerrainClimateGPU::ValidateContract(
		const std::filesystem::path& sourceShaderPath,
		const std::filesystem::path& advectionShaderPath,
		const std::filesystem::path& responseShaderPath)
	{
		TerrainClimateGPUValidationResult result;
		result.Attempted = true;
		const Ref<Texture2D> flatHeight = MakeClimateInputTexture(
			3, 1, { 0.0f, 0.0f, 0.0f });
		const Ref<Texture2D> risingHeight = MakeClimateInputTexture(
			3, 1, { 0.0f, 0.5f, 1.0f });

		auto configureTransport = [](TerrainClimateGPU& climate) {
			auto& settings = climate.GetSettings();
			settings.FixedTimeStep = 1.0f;
			settings.MaxSubsteps = 4;
			settings.WindVelocity = { 1.0f, 0.0f };
			settings.TemperatureRelaxationRate = 0.0f;
			settings.EvaporationRate = 0.0f;
			settings.CondensationRate = 0.0f;
			settings.OrographicRainRate = 0.0f;
			settings.VegetationResponseRate = 0.0f;
			settings.SaturationMoistureDepth = 10.0f;
		};

		TerrainClimateGPU transport(3, 1,
			sourceShaderPath, advectionShaderPath, responseShaderPath);
		configureTransport(transport);
		transport.SetAtmosphericMoisture({ 1.0f, 0.0f, 0.0f });
		transport.SingleStep(flatHeight, nullptr, 1.0f, 2.0f);
		std::vector<float> transported(3);
		transport.GetAtmosphericMoistureTexture()->GetImageData(
			transported.data(),
			static_cast<uint32_t>(transported.size() * sizeof(float)));
		result.DownwindMoisture = transported[1];
		result.WindTransportValid = transported[1] > 0.49f
			&& transported[0] < transported[1];

		TerrainClimateGPU rising(3, 1,
			sourceShaderPath, advectionShaderPath, responseShaderPath);
		TerrainClimateGPU flat(3, 1,
			sourceShaderPath, advectionShaderPath, responseShaderPath);
		configureTransport(rising);
		configureTransport(flat);
		rising.GetSettings().OrographicRainRate = 1.0f;
		flat.GetSettings().OrographicRainRate = 1.0f;
		rising.SetAtmosphericMoisture({ 0.1f, 0.1f, 0.1f });
		flat.SetAtmosphericMoisture({ 0.1f, 0.1f, 0.1f });
		rising.SingleStep(risingHeight, nullptr, 2.0f, 2.0f);
		flat.SingleStep(flatHeight, nullptr, 2.0f, 2.0f);
		std::vector<float> risingRain(3);
		std::vector<float> flatRain(3);
		rising.GetRainfallTexture()->GetImageData(risingRain.data(),
			static_cast<uint32_t>(risingRain.size() * sizeof(float)));
		flat.GetRainfallTexture()->GetImageData(flatRain.data(),
			static_cast<uint32_t>(flatRain.size() * sizeof(float)));
		result.RisingTerrainRainfall =
			*std::max_element(risingRain.begin(), risingRain.end());
		result.FlatTerrainRainfall =
			*std::max_element(flatRain.begin(), flatRain.end());
		result.OrographicRainValid =
			result.RisingTerrainRainfall > result.FlatTerrainRainfall + 1.0e-6f;

		TerrainClimateGPU largeFrames(3, 1,
			sourceShaderPath, advectionShaderPath, responseShaderPath);
		TerrainClimateGPU smallFrames(3, 1,
			sourceShaderPath, advectionShaderPath, responseShaderPath);
		configureTransport(largeFrames);
		configureTransport(smallFrames);
		largeFrames.GetSettings().FixedTimeStep = 0.25f;
		smallFrames.GetSettings().FixedTimeStep = 0.25f;
		largeFrames.GetSettings().WindVelocity = { 0.5f, 0.0f };
		smallFrames.GetSettings().WindVelocity = { 0.5f, 0.0f };
		largeFrames.SetAtmosphericMoisture({ 1.0f, 0.0f, 0.0f });
		smallFrames.SetAtmosphericMoisture({ 1.0f, 0.0f, 0.0f });
		largeFrames.Advance(1.0f, flatHeight, nullptr, 1.0f, 2.0f);
		for (uint32_t frame = 0; frame < 4; ++frame)
			smallFrames.Advance(0.25f, flatHeight, nullptr, 1.0f, 2.0f);
		std::vector<float> largeMoisture(3);
		std::vector<float> smallMoisture(3);
		largeFrames.GetAtmosphericMoistureTexture()->GetImageData(
			largeMoisture.data(),
			static_cast<uint32_t>(largeMoisture.size() * sizeof(float)));
		smallFrames.GetAtmosphericMoistureTexture()->GetImageData(
			smallMoisture.data(),
			static_cast<uint32_t>(smallMoisture.size() * sizeof(float)));
		for (size_t index = 0; index < largeMoisture.size(); ++index)
		{
			result.MaximumPartitionDifference = std::max(
				result.MaximumPartitionDifference,
				std::abs(largeMoisture[index] - smallMoisture[index]));
		}
		result.FramePartitionIndependent =
			result.MaximumPartitionDifference <= 1.0e-6f
			&& largeFrames.GetStatistics().StepCount
				== smallFrames.GetStatistics().StepCount;
		result.Finite = std::all_of(transported.begin(), transported.end(),
			[](float value) { return std::isfinite(value) && value >= 0.0f; })
			&& std::all_of(risingRain.begin(), risingRain.end(),
				[](float value) {
					return std::isfinite(value) && value >= 0.0f;
				})
			&& std::all_of(largeMoisture.begin(), largeMoisture.end(),
				[](float value) {
					return std::isfinite(value) && value >= 0.0f;
				});
		result.Passed = result.Finite && result.WindTransportValid
			&& result.OrographicRainValid
			&& result.FramePartitionIndependent;
		std::ostringstream message;
		message << (result.Passed ? "PASS" : "FAIL")
			<< ": downwindMoisture=" << result.DownwindMoisture
			<< ", risingRain=" << result.RisingTerrainRainfall
			<< ", flatRain=" << result.FlatTerrainRainfall
			<< ", partitionDelta=" << result.MaximumPartitionDifference;
		result.Message = message.str();
		return result;
	}

	void TerrainClimateGPU::Step(
		const Ref<Texture2D>& heightMap,
		const Ref<Texture2D>& surfaceWater,
		float heightScale, float worldSize)
	{
		const auto& specification = m_Temperature.GetSpecification();
		if (!heightMap || heightMap->GetFormat() != TextureFormat::R32F
			|| heightMap->GetWidth() != specification.Width
			|| heightMap->GetHeight() != specification.Height)
			return;
		const Ref<Texture2D>& water = ResolveSurfaceWater(surfaceWater);
		const float deltaSeconds = std::max(m_Settings.FixedTimeStep, 1.0e-6f);
		const float cellSize = std::max(worldSize, 0.0001f)
			/ std::max(specification.Width - 1u, 1u);

		m_SourceShader->Bind();
		m_SourceShader->UploadUniformFloat("u_DeltaTime", deltaSeconds);
		m_SourceShader->UploadUniformFloat("u_HeightScale", heightScale);
		m_SourceShader->UploadUniformFloat(
			"u_SeaLevelTemperature", m_Settings.SeaLevelTemperature);
		m_SourceShader->UploadUniformFloat(
			"u_TemperatureLapseRate",
			std::max(m_Settings.TemperatureLapseRate, 0.0f));
		m_SourceShader->UploadUniformFloat(
			"u_TemperatureRelaxationRate",
			std::max(m_Settings.TemperatureRelaxationRate, 0.0f));
		m_SourceShader->UploadUniformFloat(
			"u_SaturationReferenceTemperature",
			m_Settings.SaturationReferenceTemperature);
		m_SourceShader->UploadUniformFloat(
			"u_SaturationMoistureDepth",
			std::max(m_Settings.SaturationMoistureDepth, 0.0f));
		m_SourceShader->UploadUniformFloat(
			"u_SaturationTemperatureSensitivity",
			std::max(m_Settings.SaturationTemperatureSensitivity, 0.0f));
		m_SourceShader->UploadUniformFloat(
			"u_EvaporationRate", std::max(m_Settings.EvaporationRate, 0.0f));
		m_SourceShader->BindImageTexture(0, heightMap->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_SourceShader->BindImageTexture(1, water->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_SourceShader->BindImageTexture(2,
			m_Temperature.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_SourceShader->BindImageTexture(3,
			m_AtmosphericMoisture.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_SourceShader->BindImageTexture(4,
			m_Temperature.WriteTexture()->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::R32F);
		m_SourceShader->BindImageTexture(5,
			m_AtmosphericMoisture.WriteTexture()->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::R32F);
		Dispatch(m_SourceShader);
		ComputeShader::Barrier();
		m_Temperature.Swap();
		m_AtmosphericMoisture.Swap();

		m_AdvectionShader->Bind();
		m_AdvectionShader->UploadUniformFloat("u_DeltaTime", deltaSeconds);
		m_AdvectionShader->UploadUniformFloat("u_CellSize", cellSize);
		m_AdvectionShader->UploadUniformFloat2(
			"u_WindVelocity", m_Settings.WindVelocity);
		m_AdvectionShader->BindImageTexture(0,
			m_AtmosphericMoisture.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_AdvectionShader->BindImageTexture(1,
			m_AtmosphericMoisture.WriteTexture()->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::R32F);
		Dispatch(m_AdvectionShader);
		ComputeShader::Barrier();
		m_AtmosphericMoisture.Swap();

		m_ResponseShader->Bind();
		m_ResponseShader->UploadUniformFloat("u_DeltaTime", deltaSeconds);
		m_ResponseShader->UploadUniformFloat("u_CellSize", cellSize);
		m_ResponseShader->UploadUniformFloat("u_HeightScale", heightScale);
		m_ResponseShader->UploadUniformFloat2(
			"u_WindVelocity", m_Settings.WindVelocity);
		m_ResponseShader->UploadUniformFloat(
			"u_SaturationReferenceTemperature",
			m_Settings.SaturationReferenceTemperature);
		m_ResponseShader->UploadUniformFloat(
			"u_SaturationMoistureDepth",
			std::max(m_Settings.SaturationMoistureDepth, 0.0f));
		m_ResponseShader->UploadUniformFloat(
			"u_SaturationTemperatureSensitivity",
			std::max(m_Settings.SaturationTemperatureSensitivity, 0.0f));
		m_ResponseShader->UploadUniformFloat(
			"u_CondensationRate", std::max(m_Settings.CondensationRate, 0.0f));
		m_ResponseShader->UploadUniformFloat(
			"u_OrographicRainRate",
			std::max(m_Settings.OrographicRainRate, 0.0f));
		m_ResponseShader->UploadUniformFloat(
			"u_VegetationResponseRate",
			std::max(m_Settings.VegetationResponseRate, 0.0f));
		m_ResponseShader->UploadUniformFloat(
			"u_VegetationOptimalTemperature",
			m_Settings.VegetationOptimalTemperature);
		m_ResponseShader->UploadUniformFloat(
			"u_VegetationTemperatureRange",
			std::max(m_Settings.VegetationTemperatureRange, 1.0e-6f));
		m_ResponseShader->UploadUniformFloat(
			"u_VegetationMoistureForFullCover",
			std::max(m_Settings.VegetationMoistureForFullCover, 1.0e-6f));
		m_ResponseShader->BindImageTexture(0, heightMap->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_ResponseShader->BindImageTexture(1, water->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_ResponseShader->BindImageTexture(2,
			m_Temperature.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_ResponseShader->BindImageTexture(3,
			m_AtmosphericMoisture.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_ResponseShader->BindImageTexture(4,
			m_VegetationPotential.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_ResponseShader->BindImageTexture(5,
			m_AtmosphericMoisture.WriteTexture()->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::R32F);
		m_ResponseShader->BindImageTexture(6, m_Rainfall->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::R32F);
		m_ResponseShader->BindImageTexture(7,
			m_VegetationPotential.WriteTexture()->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::R32F);
		Dispatch(m_ResponseShader);
		ComputeShader::Barrier();
		m_AtmosphericMoisture.Swap();
		m_VegetationPotential.Swap();

		++m_Statistics.StepCount;
		m_Statistics.SimulatedTime += deltaSeconds;
		m_Statistics.ReadbackAvailable = false;
	}

	const Ref<Texture2D>& TerrainClimateGPU::ResolveSurfaceWater(
		const Ref<Texture2D>& surfaceWater) const
	{
		const auto& specification = m_Temperature.GetSpecification();
		if (surfaceWater && surfaceWater->GetFormat() == TextureFormat::R32F
			&& surfaceWater->GetWidth() == specification.Width
			&& surfaceWater->GetHeight() == specification.Height)
			return surfaceWater;
		return m_ZeroSurfaceWater;
	}

	void TerrainClimateGPU::Dispatch(const Ref<ComputeShader>& shader) const
	{
		const auto& specification = m_Temperature.GetSpecification();
		shader->Dispatch((specification.Width + 7u) / 8u,
			(specification.Height + 7u) / 8u, 1);
	}

}
