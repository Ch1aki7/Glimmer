#pragma once

#include "Glimmer/Renderer/ComputeShader.h"
#include "Glimmer/Simulation/SimulationGrid.h"

namespace gl {

	struct TerrainNoiseSettings {
		int Seed = 1;
		int Octaves = 7;
		float Frequency = 2.2f;
		float Lacunarity = 2.0f;
		float Persistence = 0.48f;
		float DomainWarp = 0.65f;
		float RidgeStrength = 0.58f;
		float ContinentScale = 0.32f;
		float ErosionStrength = 0.18f;
		float DetailStrength = 0.07f;
		glm::vec2 Offset = { 0.0f, 0.0f };
	};

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


