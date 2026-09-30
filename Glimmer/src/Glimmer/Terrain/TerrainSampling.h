#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace gl {

	// Terrain positions are local X/Z units. No implicit metres or Transform scale.
	enum class TerrainSampleLayout { EndpointNodes, CellCenters };

	struct TerrainSamplingGrid
	{
		uint32_t Count;
		float WorldSize;
		TerrainSampleLayout Layout;

		TerrainSamplingGrid(uint32_t count, float worldSize, TerrainSampleLayout layout)
			: Count(count), WorldSize(worldSize), Layout(layout)
		{
			if (count == 0 || !std::isfinite(worldSize) || worldSize <= 0.0f
				|| (layout == TerrainSampleLayout::EndpointNodes && count < 2))
				throw std::invalid_argument("Invalid terrain sampling grid.");
		}

		float Spacing() const
		{
			return WorldSize / static_cast<float>(
				Layout == TerrainSampleLayout::EndpointNodes ? Count - 1u : Count);
		}
		float Position(uint32_t index) const
		{
			if (index >= Count) throw std::out_of_range("Terrain sample index.");
			return -WorldSize * 0.5f + (static_cast<float>(index)
				+ (Layout == TerrainSampleLayout::CellCenters ? 0.5f : 0.0f)) * Spacing();
		}
		float TextureUV(float localPosition) const
		{
			const float uv = localPosition / WorldSize + 0.5f;
			return Layout == TerrainSampleLayout::EndpointNodes
				? (uv * static_cast<float>(Count - 1u) + 0.5f) / Count : uv;
		}
	};

	inline float TerrainStableSlope(float degrees)
	{
		return std::isfinite(degrees) ? std::clamp(degrees, 0.0f, 80.0f) : 35.0f;
	}

	inline float TerrainTalusHeight(float degrees, float distance, float heightScale)
	{
		if (!std::isfinite(distance) || distance <= 0.0f
			|| !std::isfinite(heightScale) || heightScale <= 0.0f) return 0.0f;
		return std::tan(TerrainStableSlope(degrees) * 0.017453292519943295f)
			* distance / heightScale;
	}

	struct TerrainPhysicalSample
	{
		glm::vec3 Normal;
		float SlopeDegrees;
		double Concavity; // Positive in bowls; local height units / XZ units squared.
	};

	inline TerrainPhysicalSample TerrainDerivePhysicalSample(double center,
		double left, double right, double down, double up, double spacing,
		double heightScale)
	{
		if (!std::isfinite(center) || !std::isfinite(left) || !std::isfinite(right)
			|| !std::isfinite(down) || !std::isfinite(up) || !std::isfinite(spacing)
			|| spacing <= 0.0 || !std::isfinite(heightScale) || heightScale < 0.0)
			throw std::invalid_argument("Invalid terrain physical sample.");
		const double dx = (right - left) * heightScale / (2.0 * spacing);
		const double dz = (up - down) * heightScale / (2.0 * spacing);
		return { glm::normalize(glm::vec3(-dx, 1.0, -dz)),
			static_cast<float>(std::atan(std::hypot(dx, dz)) * 57.29577951308232),
			(left + right + down + up - 4.0 * center) * heightScale / (spacing * spacing) };
	}

	// All operands are absolute local heights except the two residuals.
	inline float TerrainComposeHeight(float base, float detail,
		float simulationInitial, float simulationCurrent)
	{
		return base + detail + (simulationCurrent - simulationInitial);
	}

	struct TerrainTextureBudget
	{
		uint64_t GenerationBytes;
		uint64_t SimulationBytes;
		uint64_t DetailBytes;
		uint64_t TotalBytes() const { return GenerationBytes + SimulationBytes + DetailBytes; }
	};

	inline TerrainTextureBudget TerrainEstimateTextureBudget(uint32_t baseResolution,
		uint32_t simulationResolution, uint32_t tileCells, uint32_t halo, uint32_t tileCount)
	{
		// Bound inputs before arithmetic; estimates exclude CPU, meshes, driver and scratch.
		if (baseResolution > 8192u || simulationResolution > 8192u
			|| tileCells > 8192u || halo > 64u || tileCount > 256u)
			throw std::invalid_argument("Terrain budget exceeds supported bounds.");
		const uint64_t tileNodes = static_cast<uint64_t>(tileCells) + 1u + 2u * halo;
		return { uint64_t(baseResolution) * baseResolution * 32u,
			uint64_t(simulationResolution) * simulationResolution * 116u,
			tileNodes * tileNodes * 28u * tileCount };
	}
}
