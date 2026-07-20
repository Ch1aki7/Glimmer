#pragma once
#include "Glimmer/Core/Core.h"
#include <vector>

namespace gl {

	// 帧缓冲附件纹理格式
	enum class FramebufferTextureFormat
	{
		None = 0,
		RGBA8,              // 标准颜色
		RED_INTEGER,        // 实体 ID 拾取
		RGBA16F,            // HDR 半精度浮点
		Depth24Stencil8,    // 深度/模板
	};

	// 单个附件规格
	struct FramebufferAttachmentSpecification
	{
		FramebufferTextureFormat Format = FramebufferTextureFormat::RGBA8;
	};

	// 帧缓冲完整规格
	struct FramebufferSpecification
	{
		uint32_t Width = 1280;
		uint32_t Height = 720;
		std::vector<FramebufferAttachmentSpecification> Attachments;
		uint32_t Samples = 1;           // MSAA 采样数，1=关闭
		bool SwapChainTarget = false;   // 直接渲染到屏幕（暂未实现）

		FramebufferSpecification() = default;
		FramebufferSpecification(uint32_t w, uint32_t h)
			: Width(w), Height(h) {}
	};

	// 帧缓冲附件内部结构（每个附件的 GPU 资源）
	struct FramebufferAttachment
	{
		uint32_t RendererID = 0;
		FramebufferTextureFormat Format = FramebufferTextureFormat::None;
	};

	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;

		// 获取附件的渲染 ID（用于 ImGui::Image 等）
		virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const = 0;
		virtual uint32_t GetDepthAttachmentRendererID() const = 0;
		virtual uint32_t GetRendererID() const = 0;

		// 读取指定附件的像素值（用于鼠标拾取等场景）
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) const = 0;
		// 清除指定附件为某个值（整数附件用 glClearBufferiv）
		virtual void ClearAttachment(uint32_t attachmentIndex, int value) = 0;

		virtual const FramebufferSpecification& GetSpecification() const = 0;

		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
	};

}
