#pragma once

#include "Glimmer/Renderer/Texture.h"

namespace gl {

	struct SimulationGridSpecification {
		uint32_t Width = 256;
		uint32_t Height = 256;
		TextureFormat Format = TextureFormat::R32F;
		TextureFilter Filter = TextureFilter::Linear;
		TextureWrap Wrap = TextureWrap::ClampToEdge;
	};

	class SimulationGrid {
	public:
		explicit SimulationGrid(const SimulationGridSpecification& specification);

		const SimulationGridSpecification& GetSpecification() const { return m_Specification; }
		const Ref<Texture2D>& ReadTexture() const { return m_Textures[m_ReadIndex]; }
		const Ref<Texture2D>& WriteTexture() const { return m_Textures[1u - m_ReadIndex]; }

		void Swap();
		void Clear(const glm::vec4& value = glm::vec4(0.0f));
		void Resize(uint32_t width, uint32_t height);

	private:
		void CreateTextures();
		SimulationGridSpecification m_Specification;
		Ref<Texture2D> m_Textures[2];
		uint32_t m_ReadIndex = 0;
	};

}

