#pragma once

#include "Glimmer/Renderer/ComputeShader.h"
#include "Glimmer/Simulation/SimulationGrid.h"
#include "Glimmer/Terrain/TerrainSettings.h"
#include <vector>

namespace gl {
	struct TerrainValidationResult
	{
		bool Valid = false;
		uint64_t Hash = 0;
		float HeightMinimum = 0.0f;
		float HeightMaximum = 0.0f;
		float HeightMean = 0.0f;
		float HeightStandardDeviation = 0.0f;
		std::string Message;
	};

	class TerrainGenerator {
	public:
		TerrainGenerator(
			const SimulationGridSpecification& gridSpecification,
			const std::string& generationShaderPath,
			const std::string& erosionShaderPath,
			const std::string& derivationShaderPath,
			const std::string& stampShaderPath = {});

		// Publish complete Height/derived resources only on success.
		bool Generate(const TerrainSpecification& specification, float worldSize);
		const std::string& GetLastGenerationError() const { return m_LastGenerationError; }
		bool HasGeneratedSurface() const { return m_HasGeneratedSurface; }
		const Ref<Texture2D>& GetRecipeClipMask() const { return m_RecipeClipMask; }
		const Ref<Texture2D>& GetProtectionMap() const { return m_ProtectionMap; }
		uint64_t ReadRecipeClippedNodeCount() const; // Explicit diagnostic readback only.
		void DeriveMapsFromHeight(const Ref<Texture2D>& heightMap,
			float heightScale, float worldSize);
		void Resize(uint32_t width, uint32_t height);
		bool ReloadShadersIfChanged();

		const Ref<Texture2D>& GetHeightMap() const { return m_HeightGrid.ReadTexture(); }
		const Ref<Texture2D>& GetNormalSlopeMap() const { return m_NormalSlopeMap; }
		const Ref<Texture2D>& GetAnalysisMap() const { return m_AnalysisMap; }
		const Ref<Texture2D>& GetMaterialWeightMap() const { return m_MaterialWeightMap; }
		uint32_t GetLastDispatchCount() const { return m_LastDispatchCount; }
		std::vector<Ref<Texture2D>> GetTextureResources() const
		{
			return { m_HeightGrid.ReadTexture(), m_HeightGrid.WriteTexture(), m_NormalSlopeMap,
				m_AnalysisMap, m_MaterialWeightMap, m_RecipeClipMask, m_ProtectionMap };
		}
		TerrainValidationResult ValidateOutputs() const;
		static TerrainValidationResult ValidateSamplingContract(
			const std::string& generationShaderPath, const std::string& erosionShaderPath,
			const std::string& derivationShaderPath);
		static TerrainValidationResult ValidateRecipeContract(
			const std::string& generationShaderPath, const std::string& erosionShaderPath,
			const std::string& derivationShaderPath);
		const SimulationGridSpecification& GetGridSpecification() const
		{
			return m_HeightGrid.GetSpecification();
		}

	private:
		TerrainGenerator(const SimulationGridSpecification& grid,
			const Ref<ComputeShader>& generation, const Ref<ComputeShader>& erosion,
			const Ref<ComputeShader>& derivation, const Ref<ComputeShader>& stamp);
		void GenerateCandidate(const TerrainSpecification& specification, float worldSize);
		void ApplyRecipe(const TerrainSpecification& specification, float worldSize);
		void CreateDerivedTextures();
		void Dispatch2D(const Ref<ComputeShader>& shader,
			bool countGenerationDispatch = true);
		void RunThermalErosion(const TerrainAuthoringSettings& settings,
			float heightScale, float worldSize);
		void DeriveMaps(float heightScale, float worldSize);
		void DeriveMaps(const Ref<Texture2D>& heightMap,
			float heightScale, float worldSize, bool countGenerationDispatch);

		SimulationGrid m_HeightGrid;
		Ref<ComputeShader> m_GenerationShader;
		Ref<ComputeShader> m_ErosionShader;
		Ref<ComputeShader> m_DerivationShader;
		Ref<ComputeShader> m_StampShader;
		std::string m_StampShaderPath;
		std::string m_LastGenerationError;
		Ref<Texture2D> m_RecipeClipMask;
		Ref<Texture2D> m_ProtectionMap;
		bool m_HasGeneratedSurface = false;
		Ref<Texture2D> m_NormalSlopeMap;
		Ref<Texture2D> m_AnalysisMap;
		Ref<Texture2D> m_MaterialWeightMap;
		uint32_t m_LastDispatchCount = 0;
		uint32_t m_DataVersion = 1;
	};

}


