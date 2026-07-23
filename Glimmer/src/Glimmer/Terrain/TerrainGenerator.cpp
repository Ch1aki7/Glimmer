#include "glpch.h"
#include "TerrainGenerator.h"

namespace gl {

	TerrainGenerator::TerrainGenerator(
		const SimulationGridSpecification& gridSpecification,
		const std::string& computeShaderPath)
		: m_HeightGrid(gridSpecification),
		  m_ComputeShader(ComputeShader::Create(computeShaderPath))
	{
		GL_CORE_ASSERT(gridSpecification.Format == TextureFormat::R32F,
			"TerrainGenerator currently requires an R32F height grid.");
	}

	void TerrainGenerator::Generate(const TerrainNoiseSettings& settings)
	{
		GL_PROFILE_FUNCTION();

		m_ComputeShader->Bind();
		m_ComputeShader->UploadUniformInt("u_Seed", settings.Seed);
		m_ComputeShader->UploadUniformInt("u_Octaves", settings.Octaves);
		m_ComputeShader->UploadUniformFloat("u_Frequency", settings.Frequency);
		m_ComputeShader->UploadUniformFloat("u_Lacunarity", settings.Lacunarity);
		m_ComputeShader->UploadUniformFloat("u_Persistence", settings.Persistence);
		m_ComputeShader->UploadUniformFloat("u_DomainWarp", settings.DomainWarp);
		m_ComputeShader->UploadUniformFloat("u_RidgeStrength", settings.RidgeStrength);
		m_ComputeShader->UploadUniformFloat("u_ContinentScale", settings.ContinentScale);
		m_ComputeShader->UploadUniformFloat("u_ErosionStrength", settings.ErosionStrength);
		m_ComputeShader->UploadUniformFloat("u_DetailStrength", settings.DetailStrength);
		m_ComputeShader->UploadUniformFloat2("u_Offset", settings.Offset);
		m_ComputeShader->BindImageTexture(
			0,
			m_HeightGrid.WriteTexture()->GetRendererID(),
			0,
			ImageAccess::Write,
			ImageFormat::R32F);

		const auto& specification = m_HeightGrid.GetSpecification();
		const uint32_t groupCountX = (specification.Width + 7u) / 8u;
		const uint32_t groupCountY = (specification.Height + 7u) / 8u;
		m_ComputeShader->Dispatch(groupCountX, groupCountY, 1);
		ComputeShader::Barrier();
		m_HeightGrid.Swap();
	}

	void TerrainGenerator::Resize(uint32_t width, uint32_t height)
	{
		m_HeightGrid.Resize(width, height);
	}

	ShaderReloadResult TerrainGenerator::ReloadShaderIfChanged()
	{
		return m_ComputeShader->ReloadIfChanged();
	}

}


