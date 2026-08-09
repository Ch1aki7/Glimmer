#pragma once

#include "Glimmer/Core/Core.h"

namespace gl {

	class GPUTimer
	{
	public:
		virtual ~GPUTimer() = default;

		virtual void Begin() = 0;
		virtual void End() = 0;
		virtual bool TryGetElapsedMilliseconds(float& milliseconds) = 0;

		static Ref<GPUTimer> Create();
	};

}
