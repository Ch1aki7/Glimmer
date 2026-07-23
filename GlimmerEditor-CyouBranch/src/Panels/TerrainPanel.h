#pragma once

#include "Glimmer/Terrain/TerrainGenerator.h"

namespace gl {

	class TerrainPanel {
	public:
		void SetContext(TerrainGenerator* generator);
		void OnUpdate();
		void OnImGuiRender();

		const TerrainNoiseSettings& GetSettings() const { return m_Settings; }

	private:
		TerrainGenerator* m_Generator = nullptr;
		TerrainNoiseSettings m_Settings;
		bool m_AutoRegenerate = true;
		bool m_Dirty = true;
	};

}
