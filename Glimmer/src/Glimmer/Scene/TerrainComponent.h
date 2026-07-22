#pragma once
#include "Glimmer/Renderer/Texture.h"

namespace gl {

	struct TerrainComponent {
		Ref<Texture2D> HeightMap;
		uint32_t GridSize = 256;
		float MaxHeight = 20.0f;
		float UVScale = 1.0f;
	};

}
