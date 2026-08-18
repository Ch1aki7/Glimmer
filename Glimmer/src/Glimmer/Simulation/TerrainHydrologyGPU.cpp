#include "glpch.h"
#include "Glimmer/Simulation/TerrainHydrologyGPU.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace gl {

	namespace {
		SimulationGridSpecification MakeGrid(
			uint32_t width, uint32_t height, TextureFormat format)
		{
			SimulationGridSpecification specification;
			specification.Width = width;
			specification.Height = height;
			specification.Format = format;
			specification.Filter = TextureFilter::Nearest;
			specification.Wrap = TextureWrap::ClampToEdge;
			return specification;
		}

		Ref<Texture2D> MakeDiagnosticTexture(uint32_t width, uint32_t height)
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
	}

	TerrainHydrologyGPU::TerrainHydrologyGPU(
		uint32_t width, uint32_t height,
		std::filesystem::path fluxShaderPath,
		std::filesystem::path updateShaderPath,
		std::filesystem::path sedimentShaderPath,
		std::filesystem::path capacityShaderPath,
		std::filesystem::path erosionShaderPath)
		: m_Height(MakeGrid(width, height, TextureFormat::R32F)),
		  m_Water(MakeGrid(width, height, TextureFormat::R32F)),
		  m_Flux(MakeGrid(width, height, TextureFormat::RGBA16F)),
		  m_Velocity(MakeGrid(width, height, TextureFormat::RGBA16F)),
		  m_Sediment(MakeGrid(width, height, TextureFormat::R32F)),
		  m_SedimentCapacity(MakeDiagnosticTexture(width, height)),
		  m_SedimentSaturation(MakeDiagnosticTexture(width, height)),
		  m_FluxShader(ComputeShader::Create(fluxShaderPath.string())),
		  m_UpdateShader(ComputeShader::Create(updateShaderPath.string())),
		  m_SedimentShader(ComputeShader::Create(sedimentShaderPath.string())),
		  m_CapacityShader(ComputeShader::Create(capacityShaderPath.string())),
		  m_ErosionShader(ComputeShader::Create(erosionShaderPath.string()))
	{
		Reset();
	}

	uint32_t TerrainHydrologyGPU::Advance(float frameDeltaSeconds,
		const Ref<Texture2D>& heightMap, float heightScale, float worldSize)
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
			Step(heightMap, heightScale, worldSize);
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

	void TerrainHydrologyGPU::SingleStep(const Ref<Texture2D>& heightMap,
		float heightScale, float worldSize)
	{
		Step(heightMap, heightScale, worldSize);
	}

	void TerrainHydrologyGPU::Reset()
	{
		if (m_InitialHeightData.empty())
			m_Height.Clear();
		else
		{
			const uint32_t dataSize = static_cast<uint32_t>(
				m_InitialHeightData.size() * sizeof(float));
			m_Height.ReadTexture()->SetData(
				m_InitialHeightData.data(), dataSize);
			m_Height.WriteTexture()->SetData(
				m_InitialHeightData.data(), dataSize);
		}
		m_Water.Clear();
		m_Flux.Clear();
		m_Velocity.Clear();
		m_Sediment.Clear();
		m_SedimentCapacity->Clear(glm::vec4(0.0f));
		m_SedimentSaturation->Clear(glm::vec4(0.0f));
		m_Accumulator = 0.0;
		m_Statistics = {};
	}

	void TerrainHydrologyGPU::SetInitialHeightMap(
		const Ref<Texture2D>& heightMap, float heightScale, float worldSize)
	{
		const auto& specification = m_Height.GetSpecification();
		if (!heightMap || heightMap->GetFormat() != TextureFormat::R32F
			|| heightMap->GetWidth() != specification.Width
			|| heightMap->GetHeight() != specification.Height)
		{
			throw std::invalid_argument(
				"GPU hydrology initial height map is invalid.");
		}

		m_InitialHeightTexture = heightMap;
		m_InitialHeightData.resize(
			static_cast<size_t>(specification.Width) * specification.Height);
		heightMap->GetImageData(m_InitialHeightData.data(),
			static_cast<uint32_t>(
				m_InitialHeightData.size() * sizeof(float)));
		Reset();
	}

	void TerrainHydrologyGPU::SetSedimentDensity(
		const std::vector<float>& sedimentDensity, float worldSize)
	{
		const auto& specification = m_Sediment.GetSpecification();
		const size_t cellCount = static_cast<size_t>(specification.Width)
			* specification.Height;
		if (sedimentDensity.size() != cellCount
			|| !std::all_of(sedimentDensity.begin(), sedimentDensity.end(),
				[](float value) {
					return std::isfinite(value) && value >= 0.0f;
				}))
		{
			throw std::invalid_argument(
				"GPU hydrology sediment field is invalid.");
		}

		const uint32_t dataSize = static_cast<uint32_t>(
			sedimentDensity.size() * sizeof(float));
		m_Sediment.ReadTexture()->SetData(sedimentDensity.data(), dataSize);
		m_Sediment.WriteTexture()->SetData(sedimentDensity.data(), dataSize);
		const double cellSize = static_cast<double>(std::max(worldSize, 0.0001f))
			/ std::max(specification.Width - 1u, 1u);
		const double cellArea = cellSize * cellSize;
		m_Statistics.InitialSedimentMass = std::accumulate(
			sedimentDensity.begin(), sedimentDensity.end(), 0.0) * cellArea;
		m_Statistics.SedimentMass = m_Statistics.InitialSedimentMass;
		m_Statistics.SedimentMassError = 0.0;
		m_Statistics.MinimumSediment = sedimentDensity.empty() ? 0.0f
			: *std::min_element(sedimentDensity.begin(), sedimentDensity.end());
		m_Statistics.MaximumSediment = sedimentDensity.empty() ? 0.0f
			: *std::max_element(sedimentDensity.begin(), sedimentDensity.end());
		m_Statistics.ReadbackAvailable = false;
		UpdateSedimentDiagnostics();
	}

	void TerrainHydrologyGPU::SetUniformSedimentDensity(
		float sedimentDensity, float worldSize)
	{
		const float density = std::isfinite(sedimentDensity)
			? std::max(sedimentDensity, 0.0f) : 0.0f;
		m_Sediment.Clear(glm::vec4(density, 0.0f, 0.0f, 0.0f));
		const auto& specification = m_Sediment.GetSpecification();
		const double cellSize = static_cast<double>(std::max(worldSize, 0.0001f))
			/ std::max(specification.Width - 1u, 1u);
		const double cellArea = cellSize * cellSize;
		m_Statistics.InitialSedimentMass = static_cast<double>(density)
			* cellArea * specification.Width * specification.Height;
		m_Statistics.SedimentMass = m_Statistics.InitialSedimentMass;
		m_Statistics.SedimentMassError = 0.0;
		m_Statistics.MinimumSediment = density;
		m_Statistics.MaximumSediment = density;
		m_Statistics.ReadbackAvailable = false;
		UpdateSedimentDiagnostics();
	}

	void TerrainHydrologyGPU::SetSedimentCapacityScale(float capacityScale)
	{
		m_Settings.SedimentCapacityScale = std::isfinite(capacityScale)
			? std::max(capacityScale, 0.0f) : 0.0f;
		UpdateSedimentDiagnostics();
	}

	void TerrainHydrologyGPU::ReadbackStatistics(
		float worldSize, float heightScale)
	{
		const auto& specification = m_Water.GetSpecification();
		const size_t cellCount = static_cast<size_t>(specification.Width)
			* specification.Height;
		std::vector<float> water(cellCount);
		std::vector<float> velocity(cellCount * 4u);
		std::vector<float> sediment(cellCount);
		std::vector<float> capacity(cellCount);
		std::vector<float> saturation(cellCount);
		std::vector<float> terrainHeight(cellCount);
		m_Water.ReadTexture()->GetImageData(
			water.data(), static_cast<uint32_t>(water.size() * sizeof(float)));
		m_Velocity.ReadTexture()->GetImageData(velocity.data(),
			static_cast<uint32_t>(velocity.size() * sizeof(float)));
		m_Sediment.ReadTexture()->GetImageData(sediment.data(),
			static_cast<uint32_t>(sediment.size() * sizeof(float)));
		m_SedimentCapacity->GetImageData(capacity.data(),
			static_cast<uint32_t>(capacity.size() * sizeof(float)));
		m_SedimentSaturation->GetImageData(saturation.data(),
			static_cast<uint32_t>(saturation.size() * sizeof(float)));
		m_Height.ReadTexture()->GetImageData(terrainHeight.data(),
			static_cast<uint32_t>(terrainHeight.size() * sizeof(float)));

		const double cellSize = static_cast<double>(std::max(worldSize, 0.0001f))
			/ std::max(specification.Width - 1u, 1u);
		const double cellArea = cellSize * cellSize;
		double volume = 0.0;
		double sedimentMass = 0.0;
		double initialTerrainMass = 0.0;
		double terrainMass = 0.0;
		double erodedMass = 0.0;
		double depositedMass = 0.0;
		float minimumWater = std::numeric_limits<float>::max();
		float maximumWater = 0.0f;
		float maximumSpeed = 0.0f;
		float minimumSediment = std::numeric_limits<float>::max();
		float maximumSediment = 0.0f;
		float minimumCapacity = std::numeric_limits<float>::max();
		float maximumCapacity = 0.0f;
		float minimumSaturation = std::numeric_limits<float>::max();
		float maximumSaturation = 0.0f;
		float minimumTerrainHeight = std::numeric_limits<float>::max();
		float maximumTerrainHeight = -std::numeric_limits<float>::max();
		const float heightUnitScale = std::max(std::abs(heightScale), 1.0e-6f);
		const float terrainDensity = std::max(m_Settings.TerrainDensity, 1.0e-6f);
		bool finite = true;
		for (size_t index = 0; index < cellCount; ++index)
		{
			const float depth = water[index];
			const float speed = glm::length(glm::vec2(
				velocity[index * 4u], velocity[index * 4u + 1u]));
			const float sedimentDensity = sediment[index];
			const float sedimentCapacity = capacity[index];
			const float sedimentSaturation = saturation[index];
			const float currentHeight = terrainHeight[index] * heightUnitScale;
			const float initialHeight = m_InitialHeightData.empty()
				? currentHeight
				: m_InitialHeightData[index] * heightUnitScale;
			finite &= std::isfinite(depth) && depth >= 0.0f
				&& std::isfinite(speed)
				&& std::isfinite(sedimentDensity) && sedimentDensity >= 0.0f
				&& std::isfinite(sedimentCapacity) && sedimentCapacity >= 0.0f
				&& std::isfinite(sedimentSaturation) && sedimentSaturation >= 0.0f
				&& std::isfinite(currentHeight)
				&& currentHeight >= initialHeight
					- m_Settings.MaximumErosionDepth - 1.0e-5f;
			volume += static_cast<double>(depth) * cellArea;
			sedimentMass += static_cast<double>(sedimentDensity) * cellArea;
			initialTerrainMass += static_cast<double>(initialHeight)
				* terrainDensity * cellArea;
			terrainMass += static_cast<double>(currentHeight)
				* terrainDensity * cellArea;
			erodedMass += static_cast<double>(
				std::max(initialHeight - currentHeight, 0.0f))
				* terrainDensity * cellArea;
			depositedMass += static_cast<double>(
				std::max(currentHeight - initialHeight, 0.0f))
				* terrainDensity * cellArea;
			minimumWater = std::min(minimumWater, depth);
			maximumWater = std::max(maximumWater, depth);
			maximumSpeed = std::max(maximumSpeed, speed);
			minimumSediment = std::min(minimumSediment, sedimentDensity);
			maximumSediment = std::max(maximumSediment, sedimentDensity);
			minimumCapacity = std::min(minimumCapacity, sedimentCapacity);
			maximumCapacity = std::max(maximumCapacity, sedimentCapacity);
			minimumSaturation = std::min(minimumSaturation, sedimentSaturation);
			maximumSaturation = std::max(maximumSaturation, sedimentSaturation);
			minimumTerrainHeight = std::min(minimumTerrainHeight, currentHeight);
			maximumTerrainHeight = std::max(maximumTerrainHeight, currentHeight);
		}
		m_Statistics.WaterVolume = volume;
		m_Statistics.MassError = volume - m_Statistics.ExpectedWaterVolume;
		m_Statistics.MinimumWaterDepth = cellCount ? minimumWater : 0.0f;
		m_Statistics.MaximumWaterDepth = maximumWater;
		m_Statistics.MaximumSpeed = maximumSpeed;
		m_Statistics.SedimentMass = sedimentMass;
		m_Statistics.SedimentMassError = sedimentMass
			- m_Statistics.InitialSedimentMass;
		m_Statistics.InitialTerrainMass = initialTerrainMass;
		m_Statistics.TerrainMass = terrainMass;
		m_Statistics.ErodedMass = erodedMass;
		m_Statistics.DepositedMass = depositedMass;
		m_Statistics.TerrainSedimentMassError = terrainMass + sedimentMass
			- initialTerrainMass - m_Statistics.InitialSedimentMass;
		m_Statistics.MinimumSediment = cellCount ? minimumSediment : 0.0f;
		m_Statistics.MaximumSediment = maximumSediment;
		m_Statistics.MinimumSedimentCapacity = cellCount ? minimumCapacity : 0.0f;
		m_Statistics.MaximumSedimentCapacity = maximumCapacity;
		m_Statistics.MinimumSedimentSaturation = cellCount ? minimumSaturation : 0.0f;
		m_Statistics.MaximumSedimentSaturation = maximumSaturation;
		m_Statistics.MinimumTerrainHeight =
			cellCount ? minimumTerrainHeight : 0.0f;
		m_Statistics.MaximumTerrainHeight =
			cellCount ? maximumTerrainHeight : 0.0f;
		m_Statistics.Finite = finite;
		m_Statistics.ReadbackAvailable = true;
	}

	bool TerrainHydrologyGPU::ReloadShadersIfChanged()
	{
		const ShaderReloadResult flux = m_FluxShader->ReloadIfChanged();
		const ShaderReloadResult update = m_UpdateShader->ReloadIfChanged();
		const ShaderReloadResult sediment = m_SedimentShader->ReloadIfChanged();
		const ShaderReloadResult capacity = m_CapacityShader->ReloadIfChanged();
		const ShaderReloadResult erosion = m_ErosionShader->ReloadIfChanged();
		return (flux.Attempted && flux.Success)
			|| (update.Attempted && update.Success)
			|| (sediment.Attempted && sediment.Success)
			|| (capacity.Attempted && capacity.Success)
			|| (erosion.Attempted && erosion.Success);
	}

	TerrainHydrologyGPUValidationResult TerrainHydrologyGPU::ValidateContract(
		const std::filesystem::path& fluxShaderPath,
		const std::filesystem::path& updateShaderPath,
		const std::filesystem::path& sedimentShaderPath,
		const std::filesystem::path& capacityShaderPath,
		const std::filesystem::path& erosionShaderPath)
	{
		TerrainHydrologyGPUValidationResult result;
		result.Attempted = true;

		TextureSpecification heightSpecification;
		heightSpecification.Width = 3;
		heightSpecification.Height = 1;
		heightSpecification.Format = TextureFormat::R32F;
		heightSpecification.MinFilter = TextureFilter::Nearest;
		heightSpecification.MagFilter = TextureFilter::Nearest;
		heightSpecification.WrapS = TextureWrap::ClampToEdge;
		heightSpecification.WrapT = TextureWrap::ClampToEdge;
		heightSpecification.Usage = TextureUsage::Sampled
			| TextureUsage::Storage | TextureUsage::Readback;
		const Ref<Texture2D> heightMap = Texture2D::Create(heightSpecification);
		const std::array<float, 3> basinHeight = { 1.0f, 0.0f, 1.0f };
		heightMap->SetData(basinHeight.data(),
			static_cast<uint32_t>(sizeof(basinHeight)));

		auto configure = [](TerrainHydrologyGPU& hydrology) {
			auto& settings = hydrology.GetSettings();
			settings.FixedTimeStep = 0.01f;
			settings.MaxSubsteps = 4;
			settings.Gravity = 9.81f;
			settings.FluxDamping = 0.98f;
			settings.RainfallRate = 0.2f;
		};

		TerrainHydrologyGPU hydrology(
			3, 1, fluxShaderPath, updateShaderPath,
			sedimentShaderPath, capacityShaderPath, erosionShaderPath);
		hydrology.SetInitialHeightMap(heightMap, 1.0f, 2.0f);
		configure(hydrology);
		hydrology.SetSedimentDensity({ 1.0f, 0.0f, 0.0f }, 2.0f);
		for (uint32_t frame = 0; frame < 25; ++frame)
			hydrology.Advance(0.04f, heightMap, 1.0f, 2.0f);
		hydrology.ReadbackStatistics(2.0f, 1.0f);
		const TerrainHydrologyGPUStatistics largeFrameStatistics =
			hydrology.GetStatistics();
		std::array<float, 3> largeWater{};
		std::array<float, 3> largeSediment{};
		std::array<float, 3> largeCapacity{};
		std::array<float, 3> largeSaturation{};
		hydrology.GetWaterTexture()->GetImageData(
			largeWater.data(), static_cast<uint32_t>(sizeof(largeWater)));
		hydrology.GetSedimentTexture()->GetImageData(
			largeSediment.data(), static_cast<uint32_t>(sizeof(largeSediment)));
		hydrology.GetSedimentCapacityTexture()->GetImageData(
			largeCapacity.data(), static_cast<uint32_t>(sizeof(largeCapacity)));
		hydrology.GetSedimentSaturationTexture()->GetImageData(
			largeSaturation.data(), static_cast<uint32_t>(sizeof(largeSaturation)));

		hydrology.Reset();
		hydrology.SetSedimentDensity({ 1.0f, 0.0f, 0.0f }, 2.0f);
		for (uint32_t frame = 0; frame < 100; ++frame)
			hydrology.Advance(0.01f, heightMap, 1.0f, 2.0f);
		hydrology.ReadbackStatistics(2.0f, 1.0f);
		const TerrainHydrologyGPUStatistics smallFrameStatistics =
			hydrology.GetStatistics();
		std::array<float, 3> smallWater{};
		std::array<float, 3> smallSediment{};
		std::array<float, 3> smallCapacity{};
		std::array<float, 3> smallSaturation{};
		hydrology.GetWaterTexture()->GetImageData(
			smallWater.data(), static_cast<uint32_t>(sizeof(smallWater)));
		hydrology.GetSedimentTexture()->GetImageData(
			smallSediment.data(), static_cast<uint32_t>(sizeof(smallSediment)));
		hydrology.GetSedimentCapacityTexture()->GetImageData(
			smallCapacity.data(), static_cast<uint32_t>(sizeof(smallCapacity)));
		hydrology.GetSedimentSaturationTexture()->GetImageData(
			smallSaturation.data(), static_cast<uint32_t>(sizeof(smallSaturation)));

		result.Finite = largeFrameStatistics.Finite
			&& smallFrameStatistics.Finite;
		const double expectedVolume = std::max(
			largeFrameStatistics.ExpectedWaterVolume, 1.0e-12);
		result.RelativeMassError = std::abs(largeFrameStatistics.MassError)
			/ expectedVolume;
		result.MassConserved = result.RelativeMassError <= 2.0e-3;
		const double initialSedimentMass = std::max(
			largeFrameStatistics.InitialSedimentMass, 1.0e-12);
		result.RelativeSedimentMassError = std::abs(
			largeFrameStatistics.SedimentMassError) / initialSedimentMass;
		result.SedimentMassConserved =
			result.RelativeSedimentMassError <= 2.0e-3;
		result.BasinDepth = largeWater[1];
		result.MaximumRimDepth = std::max(largeWater[0], largeWater[2]);
		result.BasinAccumulation = result.BasinDepth
			> result.MaximumRimDepth + 1.0e-4f;
		result.SourceSediment = largeSediment[0];
		result.DownstreamSediment = largeSediment[1];
		result.SedimentMovedDownstream = result.DownstreamSediment
			> result.SourceSediment + 1.0e-4f;
		result.MaximumSedimentCapacity = largeFrameStatistics.MaximumSedimentCapacity;
		result.MaximumSedimentSaturation = largeFrameStatistics.MaximumSedimentSaturation;
		result.SedimentCapacityValid = result.MaximumSedimentCapacity > 0.0f
			&& result.MaximumSedimentSaturation <= 1000.0f;
		for (size_t index = 0; index < largeWater.size(); ++index)
		{
			result.MaximumPartitionDifference = std::max(
				result.MaximumPartitionDifference,
				std::abs(largeWater[index] - smallWater[index]));
			result.MaximumSedimentPartitionDifference = std::max(
				result.MaximumSedimentPartitionDifference,
				std::abs(largeSediment[index] - smallSediment[index]));
			result.MaximumCapacityPartitionDifference = std::max(
				result.MaximumCapacityPartitionDifference,
				std::abs(largeCapacity[index] - smallCapacity[index]));
			result.MaximumSaturationPartitionDifference = std::max(
				result.MaximumSaturationPartitionDifference,
				std::abs(largeSaturation[index] - smallSaturation[index]));
		}
		result.FramePartitionIndependent =
			largeFrameStatistics.StepCount == 100
			&& smallFrameStatistics.StepCount == 100
			&& result.MaximumPartitionDifference <= 5.0e-4f;
		result.SedimentFramePartitionIndependent =
			largeFrameStatistics.StepCount == 100
			&& smallFrameStatistics.StepCount == 100
			&& result.MaximumSedimentPartitionDifference <= 5.0e-4f;
		result.SedimentCapacityFramePartitionIndependent =
			largeFrameStatistics.StepCount == 100
			&& smallFrameStatistics.StepCount == 100
			&& result.MaximumCapacityPartitionDifference <= 5.0e-4f
			&& result.MaximumSaturationPartitionDifference <= 5.0e-4f;

		TerrainHydrologyGPU erosionHydrology(
			3, 1, fluxShaderPath, updateShaderPath,
			sedimentShaderPath, capacityShaderPath, erosionShaderPath);
		erosionHydrology.SetInitialHeightMap(heightMap, 1.0f, 2.0f);
		configure(erosionHydrology);
		auto& erosionSettings = erosionHydrology.GetSettings();
		erosionSettings.SedimentCapacityScale = 10.0f;
		erosionSettings.ErosionRate = 5.0f;
		erosionSettings.DepositionRate = 5.0f;
		erosionSettings.TerrainDensity = 1.0f;
		erosionSettings.MaximumErosionDepth = 0.02f;
		erosionSettings.MaximumHeightChangePerStep = 0.001f;
		erosionHydrology.SetSedimentDensity({ 1.0f, 0.0f, 0.0f }, 2.0f);
		for (uint32_t frame = 0; frame < 25; ++frame)
			erosionHydrology.Advance(0.04f, heightMap, 1.0f, 2.0f);
		erosionHydrology.ReadbackStatistics(2.0f, 1.0f);
		const TerrainHydrologyGPUStatistics erosionLargeStatistics =
			erosionHydrology.GetStatistics();
		std::array<float, 3> erosionLargeHeight{};
		std::array<float, 3> erosionLargeSediment{};
		erosionHydrology.GetHeightTexture()->GetImageData(
			erosionLargeHeight.data(),
			static_cast<uint32_t>(sizeof(erosionLargeHeight)));
		erosionHydrology.GetSedimentTexture()->GetImageData(
			erosionLargeSediment.data(),
			static_cast<uint32_t>(sizeof(erosionLargeSediment)));

		erosionHydrology.Reset();
		std::array<float, 3> resetHeight{};
		erosionHydrology.GetHeightTexture()->GetImageData(
			resetHeight.data(), static_cast<uint32_t>(sizeof(resetHeight)));
		result.ErosionResetValid = true;
		for (size_t index = 0; index < resetHeight.size(); ++index)
			result.ErosionResetValid &= std::abs(
				resetHeight[index] - basinHeight[index]) <= 1.0e-6f;

		erosionHydrology.SetSedimentDensity({ 1.0f, 0.0f, 0.0f }, 2.0f);
		for (uint32_t frame = 0; frame < 100; ++frame)
			erosionHydrology.Advance(0.01f, heightMap, 1.0f, 2.0f);
		erosionHydrology.ReadbackStatistics(2.0f, 1.0f);
		const TerrainHydrologyGPUStatistics erosionSmallStatistics =
			erosionHydrology.GetStatistics();
		std::array<float, 3> erosionSmallHeight{};
		std::array<float, 3> erosionSmallSediment{};
		erosionHydrology.GetHeightTexture()->GetImageData(
			erosionSmallHeight.data(),
			static_cast<uint32_t>(sizeof(erosionSmallHeight)));
		erosionHydrology.GetSedimentTexture()->GetImageData(
			erosionSmallSediment.data(),
			static_cast<uint32_t>(sizeof(erosionSmallSediment)));

		const double initialCombinedMass = std::max(
			std::abs(erosionLargeStatistics.InitialTerrainMass)
				+ erosionLargeStatistics.InitialSedimentMass,
			1.0e-12);
		result.RelativeTerrainSedimentMassError = std::abs(
			erosionLargeStatistics.TerrainSedimentMassError)
			/ initialCombinedMass;
		for (size_t index = 0; index < basinHeight.size(); ++index)
		{
			result.ErodedHeight = std::max(result.ErodedHeight,
				basinHeight[index] - erosionLargeHeight[index]);
			result.DepositedHeight = std::max(result.DepositedHeight,
				erosionLargeHeight[index] - basinHeight[index]);
			result.MaximumErosionHeightPartitionDifference = std::max(
				result.MaximumErosionHeightPartitionDifference,
				std::abs(
					erosionLargeHeight[index] - erosionSmallHeight[index]));
			result.MaximumErosionSedimentPartitionDifference = std::max(
				result.MaximumErosionSedimentPartitionDifference,
				std::abs(erosionLargeSediment[index]
					- erosionSmallSediment[index]));
		}
		result.ErosionDepositionValid = erosionLargeStatistics.Finite
			&& erosionSmallStatistics.Finite
			&& result.ErodedHeight > 0.0f
			&& result.DepositedHeight > 0.0f
			&& result.ErodedHeight <= erosionSettings.MaximumErosionDepth + 1.0e-5f
			&& result.RelativeTerrainSedimentMassError <= 2.0e-3;
		result.ErosionDepositionFramePartitionIndependent =
			erosionLargeStatistics.StepCount == 100
			&& erosionSmallStatistics.StepCount == 100
			&& result.MaximumErosionHeightPartitionDifference <= 5.0e-4f
			&& result.MaximumErosionSedimentPartitionDifference <= 5.0e-4f;
		result.Passed = result.Finite && result.MassConserved
			&& result.BasinAccumulation
			&& result.FramePartitionIndependent
			&& result.SedimentMassConserved
			&& result.SedimentMovedDownstream
			&& result.SedimentFramePartitionIndependent
			&& result.SedimentCapacityValid
			&& result.SedimentCapacityFramePartitionIndependent
			&& result.ErosionDepositionValid
			&& result.ErosionDepositionFramePartitionIndependent
			&& result.ErosionResetValid;

		std::ostringstream message;
		message << (result.Passed ? "PASS" : "FAIL")
			<< ": finite=" << result.Finite
			<< ", relativeMassError=" << result.RelativeMassError
			<< ", basin=" << result.BasinDepth
			<< ", rimMax=" << result.MaximumRimDepth
			<< ", partitionDelta=" << result.MaximumPartitionDifference
			<< ", sedimentMassError=" << result.RelativeSedimentMassError
			<< ", sedimentSource=" << result.SourceSediment
			<< ", sedimentDownstream=" << result.DownstreamSediment
			<< ", sedimentPartitionDelta="
			<< result.MaximumSedimentPartitionDifference
			<< ", capacityMax=" << result.MaximumSedimentCapacity
			<< ", saturationMax=" << result.MaximumSedimentSaturation
			<< ", capacityPartitionDelta="
			<< result.MaximumCapacityPartitionDifference
			<< ", saturationPartitionDelta="
			<< result.MaximumSaturationPartitionDifference
			<< ", erosionMassError="
			<< result.RelativeTerrainSedimentMassError
			<< ", erodedHeight=" << result.ErodedHeight
			<< ", depositedHeight=" << result.DepositedHeight
			<< ", erosionHeightPartitionDelta="
			<< result.MaximumErosionHeightPartitionDifference
			<< ", erosionSedimentPartitionDelta="
			<< result.MaximumErosionSedimentPartitionDifference
			<< ", erosionReset=" << result.ErosionResetValid;
		result.Message = message.str();
		return result;
	}

	void TerrainHydrologyGPU::Step(const Ref<Texture2D>& heightMap,
		float heightScale, float worldSize)
	{
		if (!heightMap || heightMap->GetFormat() != TextureFormat::R32F
			|| !m_InitialHeightTexture || m_InitialHeightData.empty())
			return;
		const float deltaSeconds = std::max(m_Settings.FixedTimeStep, 1.0e-6f);
		const float cellSize = std::max(worldSize, 0.0001f)
			/ std::max(m_Height.GetSpecification().Width - 1u, 1u);

		m_FluxShader->Bind();
		m_FluxShader->UploadUniformFloat("u_DeltaTime", deltaSeconds);
		m_FluxShader->UploadUniformFloat("u_CellSize", cellSize);
		m_FluxShader->UploadUniformFloat("u_HeightScale", heightScale);
		m_FluxShader->UploadUniformFloat("u_Gravity", std::max(m_Settings.Gravity, 0.0f));
		m_FluxShader->UploadUniformFloat("u_FluxDamping",
			glm::clamp(m_Settings.FluxDamping, 0.0f, 1.0f));
		m_FluxShader->UploadUniformFloat("u_RainfallRate",
			std::max(m_Settings.RainfallRate, 0.0f));
		m_FluxShader->BindImageTexture(0,
			m_Height.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_FluxShader->BindImageTexture(1, m_Water.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_FluxShader->BindImageTexture(2, m_Flux.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::RGBA16F);
		m_FluxShader->BindImageTexture(3, m_Flux.WriteTexture()->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::RGBA16F);
		Dispatch(m_FluxShader);
		ComputeShader::Barrier();
		m_Flux.Swap();

		m_UpdateShader->Bind();
		m_UpdateShader->UploadUniformFloat("u_DeltaTime", deltaSeconds);
		m_UpdateShader->UploadUniformFloat("u_CellSize", cellSize);
		m_UpdateShader->UploadUniformFloat("u_RainfallRate",
			std::max(m_Settings.RainfallRate, 0.0f));
		m_UpdateShader->BindImageTexture(0, m_Water.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_UpdateShader->BindImageTexture(1, m_Flux.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::RGBA16F);
		m_UpdateShader->BindImageTexture(2, m_Water.WriteTexture()->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::R32F);
		m_UpdateShader->BindImageTexture(3, m_Velocity.WriteTexture()->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::RGBA16F);
		Dispatch(m_UpdateShader);
		ComputeShader::Barrier();

		m_SedimentShader->Bind();
		m_SedimentShader->UploadUniformFloat("u_DeltaTime", deltaSeconds);
		m_SedimentShader->UploadUniformFloat("u_CellSize", cellSize);
		m_SedimentShader->UploadUniformFloat("u_RainfallRate",
			std::max(m_Settings.RainfallRate, 0.0f));
		m_SedimentShader->BindImageTexture(0,
			m_Water.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_SedimentShader->BindImageTexture(1,
			m_Flux.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::RGBA16F);
		m_SedimentShader->BindImageTexture(2,
			m_Sediment.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_SedimentShader->BindImageTexture(3,
			m_Sediment.WriteTexture()->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::R32F);
		Dispatch(m_SedimentShader);
		ComputeShader::Barrier();

		m_Water.Swap();
		m_Velocity.Swap();
		m_Sediment.Swap();
		UpdateSedimentDiagnostics();
		ApplyErosionDeposition(deltaSeconds, heightScale);
		UpdateSedimentDiagnostics();

		++m_Statistics.StepCount;
		m_Statistics.SimulatedTime += deltaSeconds;
		const double cellArea = static_cast<double>(cellSize) * cellSize;
		m_Statistics.ExpectedWaterVolume +=
			static_cast<double>(std::max(m_Settings.RainfallRate, 0.0f))
			* deltaSeconds * cellArea * m_Water.GetSpecification().Width
			* m_Water.GetSpecification().Height;
		m_Statistics.ReadbackAvailable = false;
	}

	void TerrainHydrologyGPU::ApplyErosionDeposition(
		float deltaSeconds, float heightScale)
	{
		const float erosionRate = std::max(m_Settings.ErosionRate, 0.0f);
		const float depositionRate = std::max(m_Settings.DepositionRate, 0.0f);
		const float maximumHeightChange = std::max(
			m_Settings.MaximumHeightChangePerStep, 0.0f);
		if ((erosionRate <= 0.0f && depositionRate <= 0.0f)
			|| maximumHeightChange <= 0.0f || !m_InitialHeightTexture)
		{
			return;
		}

		m_ErosionShader->Bind();
		m_ErosionShader->UploadUniformFloat(
			"u_DeltaTime", std::max(deltaSeconds, 0.0f));
		m_ErosionShader->UploadUniformFloat(
			"u_HeightScale", std::max(std::abs(heightScale), 1.0e-6f));
		m_ErosionShader->UploadUniformFloat(
			"u_ErosionRate", erosionRate);
		m_ErosionShader->UploadUniformFloat(
			"u_DepositionRate", depositionRate);
		m_ErosionShader->UploadUniformFloat(
			"u_TerrainDensity", std::max(m_Settings.TerrainDensity, 1.0e-6f));
		m_ErosionShader->UploadUniformFloat(
			"u_MaximumErosionDepth",
			std::max(m_Settings.MaximumErosionDepth, 0.0f));
		m_ErosionShader->UploadUniformFloat(
			"u_MaximumHeightChange", maximumHeightChange);
		m_ErosionShader->BindImageTexture(0,
			m_InitialHeightTexture->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_ErosionShader->BindImageTexture(1,
			m_Height.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_ErosionShader->BindImageTexture(2,
			m_Sediment.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_ErosionShader->BindImageTexture(3,
			m_SedimentCapacity->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_ErosionShader->BindImageTexture(4,
			m_Height.WriteTexture()->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::R32F);
		m_ErosionShader->BindImageTexture(5,
			m_Sediment.WriteTexture()->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::R32F);
		Dispatch(m_ErosionShader);
		ComputeShader::Barrier();
		m_Height.Swap();
		m_Sediment.Swap();
		m_Statistics.ReadbackAvailable = false;
	}

	void TerrainHydrologyGPU::UpdateSedimentDiagnostics()
	{
		m_CapacityShader->Bind();
		m_CapacityShader->UploadUniformFloat("u_CapacityScale",
			std::max(m_Settings.SedimentCapacityScale, 0.0f));
		m_CapacityShader->BindImageTexture(0,
			m_Water.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_CapacityShader->BindImageTexture(1,
			m_Velocity.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::RGBA16F);
		m_CapacityShader->BindImageTexture(2,
			m_Sediment.ReadTexture()->GetRendererID(), 0,
			ImageAccess::Read, ImageFormat::R32F);
		m_CapacityShader->BindImageTexture(3,
			m_SedimentCapacity->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::R32F);
		m_CapacityShader->BindImageTexture(4,
			m_SedimentSaturation->GetRendererID(), 0,
			ImageAccess::Write, ImageFormat::R32F);
		Dispatch(m_CapacityShader);
		ComputeShader::Barrier();
		m_Statistics.ReadbackAvailable = false;
	}

	void TerrainHydrologyGPU::Dispatch(const Ref<ComputeShader>& shader) const
	{
		const auto& specification = m_Water.GetSpecification();
		shader->Dispatch((specification.Width + 7u) / 8u,
			(specification.Height + 7u) / 8u, 1);
	}

}
