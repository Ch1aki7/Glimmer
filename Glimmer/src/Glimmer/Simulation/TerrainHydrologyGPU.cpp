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
	}

	TerrainHydrologyGPU::TerrainHydrologyGPU(
		uint32_t width, uint32_t height,
		std::filesystem::path fluxShaderPath,
		std::filesystem::path updateShaderPath,
		std::filesystem::path sedimentShaderPath)
		: m_Water(MakeGrid(width, height, TextureFormat::R32F)),
		  m_Flux(MakeGrid(width, height, TextureFormat::RGBA16F)),
		  m_Velocity(MakeGrid(width, height, TextureFormat::RGBA16F)),
		  m_Sediment(MakeGrid(width, height, TextureFormat::R32F)),
		  m_FluxShader(ComputeShader::Create(fluxShaderPath.string())),
		  m_UpdateShader(ComputeShader::Create(updateShaderPath.string())),
		  m_SedimentShader(ComputeShader::Create(sedimentShaderPath.string()))
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
		m_Water.Clear();
		m_Flux.Clear();
		m_Velocity.Clear();
		m_Sediment.Clear();
		m_Accumulator = 0.0;
		m_Statistics = {};
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
	}

	void TerrainHydrologyGPU::ReadbackStatistics(float worldSize)
	{
		const auto& specification = m_Water.GetSpecification();
		const size_t cellCount = static_cast<size_t>(specification.Width)
			* specification.Height;
		std::vector<float> water(cellCount);
		std::vector<float> velocity(cellCount * 4u);
		std::vector<float> sediment(cellCount);
		m_Water.ReadTexture()->GetImageData(
			water.data(), static_cast<uint32_t>(water.size() * sizeof(float)));
		m_Velocity.ReadTexture()->GetImageData(velocity.data(),
			static_cast<uint32_t>(velocity.size() * sizeof(float)));
		m_Sediment.ReadTexture()->GetImageData(sediment.data(),
			static_cast<uint32_t>(sediment.size() * sizeof(float)));

		const double cellSize = static_cast<double>(std::max(worldSize, 0.0001f))
			/ std::max(specification.Width - 1u, 1u);
		const double cellArea = cellSize * cellSize;
		double volume = 0.0;
		double sedimentMass = 0.0;
		float minimumWater = std::numeric_limits<float>::max();
		float maximumWater = 0.0f;
		float maximumSpeed = 0.0f;
		float minimumSediment = std::numeric_limits<float>::max();
		float maximumSediment = 0.0f;
		bool finite = true;
		for (size_t index = 0; index < cellCount; ++index)
		{
			const float depth = water[index];
			const float speed = glm::length(glm::vec2(
				velocity[index * 4u], velocity[index * 4u + 1u]));
			const float sedimentDensity = sediment[index];
			finite &= std::isfinite(depth) && depth >= 0.0f
				&& std::isfinite(speed)
				&& std::isfinite(sedimentDensity) && sedimentDensity >= 0.0f;
			volume += static_cast<double>(depth) * cellArea;
			sedimentMass += static_cast<double>(sedimentDensity) * cellArea;
			minimumWater = std::min(minimumWater, depth);
			maximumWater = std::max(maximumWater, depth);
			maximumSpeed = std::max(maximumSpeed, speed);
			minimumSediment = std::min(minimumSediment, sedimentDensity);
			maximumSediment = std::max(maximumSediment, sedimentDensity);
		}
		m_Statistics.WaterVolume = volume;
		m_Statistics.MassError = volume - m_Statistics.ExpectedWaterVolume;
		m_Statistics.MinimumWaterDepth = cellCount ? minimumWater : 0.0f;
		m_Statistics.MaximumWaterDepth = maximumWater;
		m_Statistics.MaximumSpeed = maximumSpeed;
		m_Statistics.SedimentMass = sedimentMass;
		m_Statistics.SedimentMassError = sedimentMass
			- m_Statistics.InitialSedimentMass;
		m_Statistics.MinimumSediment = cellCount ? minimumSediment : 0.0f;
		m_Statistics.MaximumSediment = maximumSediment;
		m_Statistics.Finite = finite;
		m_Statistics.ReadbackAvailable = true;
	}

	bool TerrainHydrologyGPU::ReloadShadersIfChanged()
	{
		const ShaderReloadResult flux = m_FluxShader->ReloadIfChanged();
		const ShaderReloadResult update = m_UpdateShader->ReloadIfChanged();
		const ShaderReloadResult sediment = m_SedimentShader->ReloadIfChanged();
		return (flux.Attempted && flux.Success)
			|| (update.Attempted && update.Success)
			|| (sediment.Attempted && sediment.Success);
	}

	TerrainHydrologyGPUValidationResult TerrainHydrologyGPU::ValidateContract(
		const std::filesystem::path& fluxShaderPath,
		const std::filesystem::path& updateShaderPath,
		const std::filesystem::path& sedimentShaderPath)
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
			3, 1, fluxShaderPath, updateShaderPath, sedimentShaderPath);
		configure(hydrology);
		hydrology.SetSedimentDensity({ 1.0f, 0.0f, 0.0f }, 2.0f);
		for (uint32_t frame = 0; frame < 25; ++frame)
			hydrology.Advance(0.04f, heightMap, 1.0f, 2.0f);
		hydrology.ReadbackStatistics(2.0f);
		const TerrainHydrologyGPUStatistics largeFrameStatistics =
			hydrology.GetStatistics();
		std::array<float, 3> largeWater{};
		std::array<float, 3> largeSediment{};
		hydrology.GetWaterTexture()->GetImageData(
			largeWater.data(), static_cast<uint32_t>(sizeof(largeWater)));
		hydrology.GetSedimentTexture()->GetImageData(
			largeSediment.data(), static_cast<uint32_t>(sizeof(largeSediment)));

		hydrology.Reset();
		hydrology.SetSedimentDensity({ 1.0f, 0.0f, 0.0f }, 2.0f);
		for (uint32_t frame = 0; frame < 100; ++frame)
			hydrology.Advance(0.01f, heightMap, 1.0f, 2.0f);
		hydrology.ReadbackStatistics(2.0f);
		const TerrainHydrologyGPUStatistics smallFrameStatistics =
			hydrology.GetStatistics();
		std::array<float, 3> smallWater{};
		std::array<float, 3> smallSediment{};
		hydrology.GetWaterTexture()->GetImageData(
			smallWater.data(), static_cast<uint32_t>(sizeof(smallWater)));
		hydrology.GetSedimentTexture()->GetImageData(
			smallSediment.data(), static_cast<uint32_t>(sizeof(smallSediment)));

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
		for (size_t index = 0; index < largeWater.size(); ++index)
		{
			result.MaximumPartitionDifference = std::max(
				result.MaximumPartitionDifference,
				std::abs(largeWater[index] - smallWater[index]));
			result.MaximumSedimentPartitionDifference = std::max(
				result.MaximumSedimentPartitionDifference,
				std::abs(largeSediment[index] - smallSediment[index]));
		}
		result.FramePartitionIndependent =
			largeFrameStatistics.StepCount == 100
			&& smallFrameStatistics.StepCount == 100
			&& result.MaximumPartitionDifference <= 5.0e-4f;
		result.SedimentFramePartitionIndependent =
			largeFrameStatistics.StepCount == 100
			&& smallFrameStatistics.StepCount == 100
			&& result.MaximumSedimentPartitionDifference <= 5.0e-4f;
		result.Passed = result.Finite && result.MassConserved
			&& result.BasinAccumulation
			&& result.FramePartitionIndependent
			&& result.SedimentMassConserved
			&& result.SedimentMovedDownstream
			&& result.SedimentFramePartitionIndependent;

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
			<< result.MaximumSedimentPartitionDifference;
		result.Message = message.str();
		return result;
	}

	void TerrainHydrologyGPU::Step(const Ref<Texture2D>& heightMap,
		float heightScale, float worldSize)
	{
		if (!heightMap || heightMap->GetFormat() != TextureFormat::R32F)
			return;
		const float deltaSeconds = std::max(m_Settings.FixedTimeStep, 1.0e-6f);
		const float cellSize = std::max(worldSize, 0.0001f)
			/ std::max(heightMap->GetWidth() - 1u, 1u);

		m_FluxShader->Bind();
		m_FluxShader->UploadUniformFloat("u_DeltaTime", deltaSeconds);
		m_FluxShader->UploadUniformFloat("u_CellSize", cellSize);
		m_FluxShader->UploadUniformFloat("u_HeightScale", heightScale);
		m_FluxShader->UploadUniformFloat("u_Gravity", std::max(m_Settings.Gravity, 0.0f));
		m_FluxShader->UploadUniformFloat("u_FluxDamping",
			glm::clamp(m_Settings.FluxDamping, 0.0f, 1.0f));
		m_FluxShader->UploadUniformFloat("u_RainfallRate",
			std::max(m_Settings.RainfallRate, 0.0f));
		m_FluxShader->BindImageTexture(0, heightMap->GetRendererID(), 0,
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

		++m_Statistics.StepCount;
		m_Statistics.SimulatedTime += deltaSeconds;
		const double cellArea = static_cast<double>(cellSize) * cellSize;
		m_Statistics.ExpectedWaterVolume +=
			static_cast<double>(std::max(m_Settings.RainfallRate, 0.0f))
			* deltaSeconds * cellArea * m_Water.GetSpecification().Width
			* m_Water.GetSpecification().Height;
		m_Statistics.ReadbackAvailable = false;
	}

	void TerrainHydrologyGPU::Dispatch(const Ref<ComputeShader>& shader) const
	{
		const auto& specification = m_Water.GetSpecification();
		shader->Dispatch((specification.Width + 7u) / 8u,
			(specification.Height + 7u) / 8u, 1);
	}

}
