#pragma once

#include "Glimmer/Renderer/ComputeShader.h"
#include "Glimmer/Simulation/SimulationGrid.h"
#include "Glimmer/Terrain/TerrainSettings.h"

namespace gl {

	class TerrainGenerator {
	public:
		TerrainGenerator(
			const SimulationGridSpecification& gridSpecification,
			const std::string& computeShaderPath);

		void Generate(const TerrainNoiseSettings& settings);
		void Resize(uint32_t width, uint32_t height);
		ShaderReloadResult ReloadShaderIfChanged();

		const Ref<Texture2D>& GetHeightMap() const { return m_HeightGrid.ReadTexture(); }
		const SimulationGridSpecification& GetGridSpecification() const
		{
			return m_HeightGrid.GetSpecification();
		}

	private:
		SimulationGrid m_HeightGrid;
		Ref<ComputeShader> m_ComputeShader;
	};

}


