#include "glpch.h"
#include "Glimmer/Simulation/TerrainHydrologyGPU.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
		std::filesystem::path updateShaderPath)
		: m_Water(MakeGrid(width, height, TextureFormat::R32F)),
		  m_Flux(MakeGrid(width, height, TextureFormat::RGBA16F)),
		  m_Velocity(MakeGrid(width, height, TextureFormat::RGBA16F)),
		  m_FluxShader(ComputeShader::Create(fluxShaderPath.string())),
		  m_UpdateShader(ComputeShader::Create(updateShaderPath.string()))
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
		m_Accumulator = 0.0;
		m_Statistics = {};
	}

	void TerrainHydrologyGPU::ReadbackStatistics(float worldSize)
	{
		const auto& specification = m_Water.GetSpecification();
		const size_t cellCount = static_cast<size_t>(specification.Width)
			* specification.Height;
		std::vector<float> water(cellCount);
		std::vector<float> velocity(cellCount * 4u);
		m_Water.ReadTexture()->GetImageData(
			water.data(), static_cast<uint32_t>(water.size() * sizeof(float)));
		m_Velocity.ReadTexture()->GetImageData(velocity.data(),
			static_cast<uint32_t>(velocity.size() * sizeof(float)));

		const double cellSize = static_cast<double>(std::max(worldSize, 0.0001f))
			/ std::max(specification.Width - 1u, 1u);
		const double cellArea = cellSize * cellSize;
		double volume = 0.0;
		float minimumWater = std::numeric_limits<float>::max();
		float maximumWater = 0.0f;
		float maximumSpeed = 0.0f;
		bool finite = true;
		for (size_t index = 0; index < cellCount; ++index)
		{
			const float depth = water[index];
			const float speed = glm::length(glm::vec2(
				velocity[index * 4u], velocity[index * 4u + 1u]));
			finite &= std::isfinite(depth) && depth >= 0.0f
				&& std::isfinite(speed);
			volume += static_cast<double>(depth) * cellArea;
			minimumWater = std::min(minimumWater, depth);
			maximumWater = std::max(maximumWater, depth);
			maximumSpeed = std::max(maximumSpeed, speed);
		}
		m_Statistics.WaterVolume = volume;
		m_Statistics.MassError = volume - m_Statistics.ExpectedWaterVolume;
		m_Statistics.MinimumWaterDepth = cellCount ? minimumWater : 0.0f;
		m_Statistics.MaximumWaterDepth = maximumWater;
		m_Statistics.MaximumSpeed = maximumSpeed;
		m_Statistics.Finite = finite;
		m_Statistics.ReadbackAvailable = true;
	}

	bool TerrainHydrologyGPU::ReloadShadersIfChanged()
	{
		const ShaderReloadResult flux = m_FluxShader->ReloadIfChanged();
		const ShaderReloadResult update = m_UpdateShader->ReloadIfChanged();
		return (flux.Attempted && flux.Success)
			|| (update.Attempted && update.Success);
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
		m_Water.Swap();
		m_Velocity.Swap();

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
