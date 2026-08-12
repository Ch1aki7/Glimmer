#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace gl {

	class FrustumCulling
	{
	public:
		static bool IntersectsClipFrustum(
			const glm::vec3& boundsMin,
			const glm::vec3& boundsMax,
			const glm::mat4& transform,
			const glm::mat4& viewProjection)
		{
			bool outsideLeft = true;
			bool outsideRight = true;
			bool outsideBottom = true;
			bool outsideTop = true;
			bool outsideNear = true;
			bool outsideFar = true;
			for (uint32_t z = 0; z < 2; ++z)
				for (uint32_t y = 0; y < 2; ++y)
					for (uint32_t x = 0; x < 2; ++x)
					{
						const glm::vec3 local(
							x ? boundsMax.x : boundsMin.x,
							y ? boundsMax.y : boundsMin.y,
							z ? boundsMax.z : boundsMin.z);
						const glm::vec4 clip = viewProjection * transform
							* glm::vec4(local, 1.0f);
						outsideLeft &= clip.x < -clip.w;
						outsideRight &= clip.x > clip.w;
						outsideBottom &= clip.y < -clip.w;
						outsideTop &= clip.y > clip.w;
						outsideNear &= clip.z < -clip.w;
						outsideFar &= clip.z > clip.w;
					}
			return !(outsideLeft || outsideRight || outsideBottom
				|| outsideTop || outsideNear || outsideFar);
		}
	};

}
