#pragma once

#include "Glimmer/Renderer/GPUTimer.h"

#include <array>
#include <cstdint>

namespace gl {

	class OpenGLGPUTimer final : public GPUTimer
	{
	public:
		OpenGLGPUTimer();
		~OpenGLGPUTimer() override;

		void Begin() override;
		void End() override;
		bool TryGetElapsedMilliseconds(float& milliseconds) override;

	private:
		static constexpr uint32_t QueryCount = 4;
		std::array<uint32_t, QueryCount> m_QueryIDs{};
		std::array<bool, QueryCount> m_Pending{};
		uint32_t m_NextQuery = 0;
		uint32_t m_ActiveQuery = 0;
		bool m_Active = false;
	};

}
