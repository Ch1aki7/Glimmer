#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>

namespace gl {

	enum class TerrainQueryStatus { Ready, NotReady, InvalidData, InvalidPosition, OutOfBounds, StaleVersion, UnsupportedSurface };

	struct TerrainSurfaceVersion
	{
		uint64_t Identity = 0;
		uint64_t Generation = 0;
		bool operator==(const TerrainSurfaceVersion& other) const
		{ return Identity == other.Identity && Generation == other.Generation; }
	};

	struct TerrainSurfaceSample
	{
		TerrainQueryStatus Status = TerrainQueryStatus::NotReady;
		float Height = 0;
		glm::vec3 Normal{ 0, 1, 0 };
		float SlopeDegrees = 0;
	};

	// Owned CPU values; Initialize replaces only on success. Not a LOD mesh or simulation snapshot.
	class TerrainSurfaceSnapshot
	{
	public:
		TerrainQueryStatus Initialize(uint32_t width, uint32_t height, float worldSize, float heightScale,
			TerrainSurfaceVersion version, std::vector<float> normalizedHeight)
		{
			if (width < 2 || height < 2 || width > 8192 || height > 8192
				|| !std::isfinite(worldSize) || worldSize <= 0 || !std::isfinite(heightScale) || heightScale < 0
				|| !version.Identity || !version.Generation || normalizedHeight.size() != size_t(width) * height
				|| !std::all_of(normalizedHeight.begin(), normalizedHeight.end(), [](float value) {
					return std::isfinite(value) && value >= 0 && value <= 1; }))
				return TerrainQueryStatus::InvalidData;
			m_Width = width; m_Height = height; m_WorldSize = worldSize; m_HeightScale = heightScale;
			m_Version = version; m_HeightValues = std::move(normalizedHeight);
			return TerrainQueryStatus::Ready;
		}

		TerrainSurfaceVersion GetVersion() const { return m_Version; }
		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }
		float GetWorldSize() const { return m_WorldSize; }
		float GetHeightScale() const { return m_HeightScale; }

		TerrainSurfaceSample Query(glm::vec2 localXZ, TerrainSurfaceVersion currentVersion) const
		{
			TerrainSurfaceSample result;
			if (m_HeightValues.empty() || !currentVersion.Identity || !currentVersion.Generation) return result;
			if (!(m_Version == currentVersion)) { result.Status = TerrainQueryStatus::StaleVersion; return result; }
			if (!std::isfinite(localXZ.x) || !std::isfinite(localXZ.y))
			{ result.Status = TerrainQueryStatus::InvalidPosition; return result; }
			const double half = double(m_WorldSize) * 0.5;
			if (localXZ.x < -half || localXZ.x > half || localXZ.y < -half || localXZ.y > half)
			{ result.Status = TerrainQueryStatus::OutOfBounds; return result; }
			const double x = (double(localXZ.x) / m_WorldSize + 0.5) * (m_Width - 1);
			const double z = (double(localXZ.y) / m_WorldSize + 0.5) * (m_Height - 1);
			// Interior grid lines use the cell on their positive side; outer endpoints use the last cell.
			const uint32_t ix = std::min(uint32_t(x), m_Width - 2), iz = std::min(uint32_t(z), m_Height - 2);
			const double tx = x - ix, tz = z - iz;
			auto at = [&](uint32_t dx, uint32_t dz) { return double(m_HeightValues[size_t(iz + dz) * m_Width + ix + dx]); };
			const double a = at(0, 0), b = at(1, 0), c = at(0, 1), d = at(1, 1);
			const double value = (a + (b - a) * tx) * (1 - tz) + (c + (d - c) * tx) * tz;
			const double dx = ((b - a) * (1 - tz) + (d - c) * tz) * m_HeightScale * (m_Width - 1) / m_WorldSize;
			const double dz = ((c - a) * (1 - tx) + (d - b) * tx) * m_HeightScale * (m_Height - 1) / m_WorldSize;
			const double length = std::sqrt(1 + dx * dx + dz * dz);
			result.Height = float(value * m_HeightScale);
			result.Normal = { float(-dx / length), float(1 / length), float(-dz / length) };
			result.SlopeDegrees = float(std::atan(std::hypot(dx, dz)) * 57.29577951308232);
			result.Status = TerrainQueryStatus::Ready;
			return result;
		}

	private:
		uint32_t m_Width = 0, m_Height = 0;
		float m_WorldSize = 0, m_HeightScale = 0;
		TerrainSurfaceVersion m_Version;
		std::vector<float> m_HeightValues;
	};
}
