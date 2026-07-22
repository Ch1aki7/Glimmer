#pragma once
#include "Glimmer/Core/Core.h"

namespace gl {

	// GPU → CPU 异步像素传输（双缓冲 PBO）
	class PixelBuffer {
	public:
		virtual ~PixelBuffer() = default;

		// 开始异步读回：不阻塞，数据在后续帧可用
		virtual void BeginRead(uint32_t textureID) = 0;

		// 获取上一帧的数据（如果上一帧开始了读回）
		virtual const void* Map() = 0;
		virtual void Unmap() = 0;

		virtual uint32_t GetSize() const = 0;
		virtual bool IsReady() const = 0;

		static Ref<PixelBuffer> Create(uint32_t width, uint32_t height, uint32_t channels);
	};

}
