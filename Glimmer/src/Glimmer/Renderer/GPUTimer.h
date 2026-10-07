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

		// Timestamp pairs support nested intervals; default retains GL_TIME_ELAPSED.
		static Ref<GPUTimer> Create(bool timestampPairs = false);
	};

}
