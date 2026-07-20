#pragma once
#include "Glimmer/Renderer/Framebuffer.h"
#include <vector>

namespace gl {

	class OpenGLFramebuffer : public Framebuffer
	{
	public:
		OpenGLFramebuffer(const FramebufferSpecification& spec);
		virtual ~OpenGLFramebuffer();

		virtual void Bind() override;
		virtual void Unbind() override;
		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const override;
		virtual uint32_t GetDepthAttachmentRendererID() const override { return m_DepthAttachment.RendererID; }
		virtual uint32_t GetRendererID() const override { return m_RendererID; }
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) const override;
		virtual void ClearAttachment(uint32_t attachmentIndex, int value) override;

		virtual const FramebufferSpecification& GetSpecification() const override { return m_Specification; }

	private:
		void Invalidate();
		void ResizeAttachments();

		FramebufferSpecification m_Specification;

		uint32_t m_RendererID = 0;
		std::vector<FramebufferAttachment> m_ColorAttachments;
		FramebufferAttachment m_DepthAttachment;

		// MSAA
		uint32_t m_MSAAColorRBO = 0;
		uint32_t m_MSAADepthRBO = 0;
	};

}
