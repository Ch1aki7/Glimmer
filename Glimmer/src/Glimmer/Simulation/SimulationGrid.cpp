#include "glpch.h"
#include "SimulationGrid.h"

namespace gl {

	SimulationGrid::SimulationGrid(const SimulationGridSpecification& specification)
		: m_Specification(specification)
	{
		GL_CORE_ASSERT(m_Specification.Width > 0 && m_Specification.Height > 0,
			"SimulationGrid dimensions must be greater than zero.");
		CreateTextures();
		Clear();
	}

	void SimulationGrid::CreateTextures()
	{
		TextureSpecification textureSpecification;
		textureSpecification.Width = m_Specification.Width;
		textureSpecification.Height = m_Specification.Height;
		textureSpecification.Format = m_Specification.Format;
		textureSpecification.MinFilter = m_Specification.Filter;
		textureSpecification.MagFilter = m_Specification.Filter;
		textureSpecification.WrapS = m_Specification.Wrap;
		textureSpecification.WrapT = m_Specification.Wrap;
		textureSpecification.Usage = TextureUsage::Sampled
			| TextureUsage::Storage
			| TextureUsage::Readback;

		m_Textures[0] = Texture2D::Create(textureSpecification);
		m_Textures[1] = Texture2D::Create(textureSpecification);
	}

	void SimulationGrid::Swap()
	{
		m_ReadIndex = 1u - m_ReadIndex;
	}

	void SimulationGrid::Clear(const glm::vec4& value)
	{
		m_Textures[0]->Clear(value);
		m_Textures[1]->Clear(value);
		m_ReadIndex = 0;
	}

	void SimulationGrid::Resize(uint32_t width, uint32_t height)
	{
		GL_CORE_ASSERT(width > 0 && height > 0,
			"SimulationGrid dimensions must be greater than zero.");
		if (m_Specification.Width == width && m_Specification.Height == height)
			return;

		m_Specification.Width = width;
		m_Specification.Height = height;
		CreateTextures();
		Clear();
	}

}
