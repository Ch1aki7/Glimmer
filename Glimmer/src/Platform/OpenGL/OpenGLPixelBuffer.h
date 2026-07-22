#pragma once
#include "Glimmer/Renderer/PixelBuffer.h"
#include <glad/glad.h>

namespace gl {

	class OpenGLPixelBuffer : public PixelBuffer {
	public:
		OpenGLPixelBuffer(uint32_t width, uint32_t height, uint32_t channels);
		virtual ~OpenGLPixelBuffer();

		virtual void BeginRead(uint32_t textureID) override;
		virtual const void* Map() override;
		virtual void Unmap() override;

		virtual uint32_t GetSize() const override { return m_Size; }
		virtual bool IsReady() const override { return m_Ready; }

	private:
		uint32_t m_PBO[2] = { 0, 0 };   // 双缓冲
		uint32_t m_Size = 0;
		uint32_t m_Width, m_Height, m_Channels;
		int m_ReadIndex = 0;              // 当前读回目标
		int m_MapIndex = -1;              // 上一次可 map 的缓冲
		bool m_Ready = false;
	};

}
