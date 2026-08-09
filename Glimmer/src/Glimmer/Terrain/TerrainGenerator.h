#pragma once

#include "Glimmer/Renderer/ComputeShader.h"
#include "Glimmer/Simulation/SimulationGrid.h"
#include "Glimmer/Terrain/TerrainSettings.h"

namespace gl {
	struct TerrainValidationResult
	{
		bool Valid = false;
		uint64_t Hash = 0;
		std::string Message;
	};

	class TerrainGenerator {
	public:
		TerrainGenerator(
			const SimulationGridSpecification& gridSpecification,
			const std::string& generationShaderPath,
			const std::string& erosionShaderPath,
			const std::string& derivationShaderPath);

		void Generate(const TerrainSpecification& specification, float worldSize);
		void Resize(uint32_t width, uint32_t height);
		bool ReloadShadersIfChanged();

		const Ref<Texture2D>& GetHeightMap() const { return m_HeightGrid.ReadTexture(); }
		const Ref<Texture2D>& GetNormalSlopeMap() const { return m_NormalSlopeMap; }
		const Ref<Texture2D>& GetAnalysisMap() const { return m_AnalysisMap; }
		const Ref<Texture2D>& GetMaterialWeightMap() const { return m_MaterialWeightMap; }
		uint32_t GetLastDispatchCount() const { return m_LastDispatchCount; }
		TerrainValidationResult ValidateOutputs() const;
		const SimulationGridSpecification& GetGridSpecification() const
		{
			return m_HeightGrid.GetSpecification();
		}

	private:
		void CreateDerivedTextures();
		void Dispatch2D(const Ref<ComputeShader>& shader);
		void RunThermalErosion(const TerrainAuthoringSettings& settings);
		void DeriveMaps(float heightScale, float worldSize);

		SimulationGrid m_HeightGrid;
		Ref<ComputeShader> m_GenerationShader;
		Ref<ComputeShader> m_ErosionShader;
		Ref<ComputeShader> m_DerivationShader;
		Ref<Texture2D> m_NormalSlopeMap;
		Ref<Texture2D> m_AnalysisMap;
		Ref<Texture2D> m_MaterialWeightMap;
		uint32_t m_LastDispatchCount = 0;
	};

}


