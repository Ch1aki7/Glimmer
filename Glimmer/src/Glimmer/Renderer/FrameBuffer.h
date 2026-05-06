#pragma once
#include "Glimmer/Core/Core.h"
#include <memory>

namespace gl {

	struct FramebufferSpecification
	{
		uint32_t Width, Height;
		uint32_t Samples = 1; // 用于多重采样抗锯齿

		bool SwapChainTarget = false; // 是否直接渲染到屏幕
	};

	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Resize(uint32_t width, uint32_t height) = 0;

		// 获取渲染出来的那个“图片”ID
		virtual uint32_t GetColorAttachmentRendererID() const = 0;

		virtual const FramebufferSpecification& GetSpecification() const = 0;

		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
	};

}
